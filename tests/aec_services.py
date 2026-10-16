# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""Encoders and decoders for the services B-AEC's Wire Tests use that wiretest 0.6.0 lacks.

Nothing here names this example: each function is one clause 21 production, ready to
fold into wiretest/ (proposed for 0.7.0). Productions are from ANSI/ASHRAE 135-2024
clause 21 (21.2 confirmed services, 21.6 base types).

    object access   create_object_request, created_object_identifier (CreateObject-ACK),
                    delete_object_request
    files           atomic_read_file_request, parse_atomic_read_file_ack,
                    atomic_write_file_request, parse_atomic_write_file_ack
    ReadRange       read_range_request, parse_read_range_ack, event_log_records
    elevator        landing_call_status (BACnetLandingCallStatus), landing_call_statuses
    values          application_signed, application_octet_string
"""
import struct

import wiretest
from wiretest import codec
from wiretest.codec import (closing, confirmed_request, context_object_id, context_unsigned, opening,
                            read_tag)

# BACnetConfirmedServiceChoice (cl. 21).
SERVICE_ATOMIC_READ_FILE = 6
SERVICE_ATOMIC_WRITE_FILE = 7
SERVICE_CREATE_OBJECT = 10
SERVICE_DELETE_OBJECT = 11
SERVICE_READ_RANGE = 26

# BACnetErrorClass / BACnetErrorCode (cl. 21).
ERROR_CLASS_DEVICE = 0
ERROR_CLASS_OBJECT = 1
ERROR_CLASS_PROPERTY = 2
ERROR_CLASS_RESOURCES = 3
ERROR_CLASS_SECURITY = 4
ERROR_CLASS_SERVICES = 5
ERROR_CODE_DYNAMIC_CREATION_NOT_SUPPORTED = 4
ERROR_CODE_NO_SPACE_FOR_OBJECT = 18
ERROR_CODE_OBJECT_DELETION_NOT_PERMITTED = 23
ERROR_CODE_OBJECT_IDENTIFIER_ALREADY_EXISTS = 24
ERROR_CODE_PASSWORD_FAILURE = 26
ERROR_CODE_UNKNOWN_OBJECT = 31
ERROR_CODE_UNSUPPORTED_OBJECT_TYPE = 36
ERROR_CODE_VALUE_OUT_OF_RANGE = 37
ERROR_CODE_WRITE_ACCESS_DENIED = 40

# ReinitializeDevice 'Reinitialized State of Device' (cl. 16.4.1.1.1).
REINITIALIZE_START_BACKUP = 2
REINITIALIZE_END_BACKUP = 3
REINITIALIZE_START_RESTORE = 4
REINITIALIZE_END_RESTORE = 5
REINITIALIZE_ABORT_RESTORE = 6

# BACnetDeviceStatus (cl. 21).
DEVICE_STATUS_OPERATIONAL = 0
DEVICE_STATUS_BACKUP_IN_PROGRESS = 5


# ---------------------------------------------------------------------------
# Values
# ---------------------------------------------------------------------------

def _signed_octets(value):
    for length in (1, 2, 3, 4):
        if -(1 << (8 * length - 1)) <= value < (1 << (8 * length - 1)):
            return value.to_bytes(length, "big", signed=True)
    raise ValueError(value)


def application_signed(value):
    """An application-tagged INTEGER (cl. 20.2.5), shortest two's-complement form."""
    body = _signed_octets(value)
    return bytes([0x30 | len(body)]) + body


def application_octet_string(octets):
    """An application-tagged OCTET STRING (cl. 20.2.8)."""
    if len(octets) < 5:
        return bytes([0x60 | len(octets)]) + octets
    if len(octets) < 254:
        return bytes([0x65, len(octets)]) + octets
    return bytes([0x65, 254]) + struct.pack(">H", len(octets)) + octets


def _context_signed(tag, value):
    body = _signed_octets(value)
    return bytes([(tag << 4) | 0x08 | len(body)]) + body


# ---------------------------------------------------------------------------
# CreateObject / DeleteObject (cl. 15.3, 15.4)
# ---------------------------------------------------------------------------

def create_object_request(invoke_id, object_type, instance=None, initial_values=()):
    """CreateObject-Request. With `instance`, the object-specifier is the object-identifier
    choice [1]; without, the object-type choice [0] (the device picks the instance).
    `initial_values`: [(property identifier, application-tagged value octets)]."""
    if instance is None:
        specifier = context_unsigned(0, object_type)
    else:
        specifier = context_object_id(1, object_type, instance)
    parameters = opening(0) + specifier + closing(0)
    if initial_values:
        parameters += opening(1)
        for property_identifier, value_octets in initial_values:
            parameters += context_unsigned(0, property_identifier) + opening(2) + value_octets + closing(2)
        parameters += closing(1)
    return confirmed_request(invoke_id, SERVICE_CREATE_OBJECT, parameters)


def created_object_identifier(answer):
    """(object type, instance) from a CreateObject-ACK (an application BACnetObjectIdentifier), or None."""
    if answer.frame is None or answer.frame.pdu_type != wiretest.PDU_COMPLEX_ACK:
        return None
    value = codec.decode_application(answer.frame.payload[3:])
    return value if isinstance(value, tuple) else None


def delete_object_request(invoke_id, object_type, instance):
    return confirmed_request(invoke_id, SERVICE_DELETE_OBJECT,
                             codec.application_object_id(object_type, instance))


# ---------------------------------------------------------------------------
# AtomicReadFile / AtomicWriteFile, stream access (cl. 14.1, 14.2)
# ---------------------------------------------------------------------------

def atomic_read_file_request(invoke_id, file_instance, start, requested_count):
    return confirmed_request(invoke_id, SERVICE_ATOMIC_READ_FILE,
                             codec.application_object_id(10, file_instance) + opening(0) +
                             application_signed(start) + codec.application_unsigned(requested_count) + closing(0))


def parse_atomic_read_file_ack(answer):
    """(end_of_file, file_start_position, file_data) from a stream-access AtomicReadFile-ACK, or None."""
    if answer.frame is None or answer.frame.pdu_type != wiretest.PDU_COMPLEX_ACK:
        return None
    apdu = answer.frame.payload
    try:
        _, _, _, eof_body, position = read_tag(apdu, 3)
        number, is_context, kind, _, position = read_tag(apdu, position)
        if not (is_context and number == 0 and kind == "open"):
            return None
        _, _, _, start_body, position = read_tag(apdu, position)
        _, _, _, data, position = read_tag(apdu, position)
        return bool(eof_body[0]), int.from_bytes(start_body, "big", signed=True), data
    except (IndexError, struct.error):
        return None


def atomic_write_file_request(invoke_id, file_instance, start, data):
    return confirmed_request(invoke_id, SERVICE_ATOMIC_WRITE_FILE,
                             codec.application_object_id(10, file_instance) + opening(0) +
                             application_signed(start) + application_octet_string(data) + closing(0))


def parse_atomic_write_file_ack(answer):
    """The file-start-position [0] of an AtomicWriteFile-ACK, or None."""
    if answer.frame is None or answer.frame.pdu_type != wiretest.PDU_COMPLEX_ACK:
        return None
    number, is_context, kind, body, _ = read_tag(answer.frame.payload, 3)
    if not (is_context and number == 0 and kind == "value"):
        return None
    return int.from_bytes(body, "big", signed=True)


# ---------------------------------------------------------------------------
# ReadRange (cl. 15.8)
# ---------------------------------------------------------------------------

def read_range_request(invoke_id, object_type, instance, property_identifier, reference_index=None, count=None):
    """ReadRange-Request; with reference_index and count, the by-position range [3];
    without, no range (the whole list)."""
    parameters = context_object_id(0, object_type, instance) + context_unsigned(1, property_identifier)
    if reference_index is not None:
        parameters += (opening(3) + codec.application_unsigned(reference_index) +
                       application_signed(count) + closing(3))
    return confirmed_request(invoke_id, SERVICE_READ_RANGE, parameters)


def parse_read_range_ack(answer):
    """A ReadRange-ACK as {result_flags, item_count, item_data (the octets inside [5])}, or None."""
    if answer.frame is None or answer.frame.pdu_type != wiretest.PDU_COMPLEX_ACK:
        return None
    apdu = answer.frame.payload
    result = {"result_flags": None, "item_count": None, "item_data": b""}
    position = 3
    try:
        while position < len(apdu):
            number, is_context, kind, body, after = read_tag(apdu, position)
            if is_context and kind == "value" and number == 3:
                result["result_flags"] = codec.decode_application(bytes([0x80 | len(body)]) + body)
            elif is_context and kind == "value" and number == 4:
                result["item_count"] = int.from_bytes(body, "big")
            elif is_context and kind == "open" and number == 5:
                start = after
                depth = 1
                tag_start = after
                while depth:
                    tag_start = after
                    number, is_context, kind, _, after = read_tag(apdu, after)
                    depth += {"open": 1, "close": -1}.get(kind, 0)
                result["item_data"] = apdu[start:tag_start]
            position = after
    except (IndexError, struct.error):
        return None
    return result


def _constructed_end(octets, position):
    """Position just after the closing tag matching an opening tag already read."""
    depth = 1
    while depth:
        _, _, kind, _, position = read_tag(octets, position)
        depth += {"open": 1, "close": -1}.get(kind, 0)
    return position


def _notification_fields(octets):
    """The top-level context-tagged primitives of a ConfirmedEventNotification-Request's
    parameters (cl. 21.2), as {tag number: value bytes}; constructed ones are skipped."""
    fields = {}
    position = 0
    while position < len(octets):
        number, is_context, kind, body, position = read_tag(octets, position)
        if kind == "open":
            position = _constructed_end(octets, position)
        elif is_context and kind == "value":
            fields[number] = body
    return fields


def event_log_records(item_data):
    """BACnetEventLogRecords (cl. 21.6) from a ReadRange-ACK's item data, each as a dict:
    kind 'status' / 'notification' / 'time-change', and for a notification its
    process_identifier, initiating_device, event_object, from_state, to_state and the
    raw notification octets."""
    records = []
    position = 0
    while position < len(item_data):
        number, is_context, kind, _, position = read_tag(item_data, position)    # [0] timestamp open
        if not (is_context and number == 0 and kind == "open"):
            break
        timestamp_start = position
        position = _constructed_end(item_data, position)
        record = {"timestamp": item_data[timestamp_start:position]}
        number, is_context, kind, _, position = read_tag(item_data, position)    # [1] log-datum open
        number, is_context, kind, body, inner = read_tag(item_data, position)    # the CHOICE
        if number == 0 and kind == "value":                                     # log-status [0] BIT STRING
            record["kind"] = "status"
            record["log_status"] = body
            position = inner
        elif number == 1 and kind == "open":                                    # notification [1] SEQUENCE
            record["kind"] = "notification"
            end = _constructed_end(item_data, inner)
            notification = item_data[inner:end]
            fields = _notification_fields(notification)

            def identifier(tag):
                if tag not in fields:
                    return None
                raw = struct.unpack(">I", fields[tag])[0]
                return raw >> 22, raw & codec.MAX_INSTANCE
            record["notification"] = notification
            record["process_identifier"] = int.from_bytes(fields.get(0, b"\x00"), "big")
            record["initiating_device"] = identifier(1)
            record["event_object"] = identifier(2)
            record["from_state"] = int.from_bytes(fields[10], "big") if 10 in fields else None
            record["to_state"] = int.from_bytes(fields[11], "big") if 11 in fields else None
            position = end
        else:
            record["kind"] = "time-change"
            position = inner
        position = read_tag(item_data, position)[4]                             # [1] log-datum close
        records.append(record)
    return records


# ---------------------------------------------------------------------------
# ReadPropertyMultiple (cl. 15.7) - one consistent snapshot of several properties
# ---------------------------------------------------------------------------

SERVICE_READ_PROPERTY_MULTIPLE = 14


def read_property_multiple_request(invoke_id, object_type, instance, references):
    """ReadPropertyMultiple-Request for one object; `references` = [(property identifier,
    array index or None)]."""
    parameters = context_object_id(0, object_type, instance) + opening(1)
    for property_identifier, array_index in references:
        parameters += context_unsigned(0, property_identifier)
        if array_index is not None:
            parameters += context_unsigned(1, array_index)
    return confirmed_request(invoke_id, SERVICE_READ_PROPERTY_MULTIPLE, parameters + closing(1))


def parse_read_property_multiple_ack(answer):
    """{(property identifier, array index or None): value octets, or (error class, code)}
    from a ReadPropertyMultiple-ACK, or None."""
    if answer.frame is None or answer.frame.pdu_type != wiretest.PDU_COMPLEX_ACK:
        return None
    apdu = answer.frame.payload
    results = {}
    position = 3
    property_identifier = None
    array_index = None
    try:
        while position < len(apdu):
            number, is_context, kind, body, position = read_tag(apdu, position)
            if is_context and kind == "value" and number == 2:
                property_identifier, array_index = int.from_bytes(body, "big"), None
            elif is_context and kind == "value" and number == 3:
                array_index = int.from_bytes(body, "big")
            elif is_context and kind == "open" and number in (4, 5):
                start = position
                end = _constructed_end(apdu, position)
                inner = apdu[start:end - 1]       # without the one-octet closing tag
                if number == 4:
                    results[(property_identifier, array_index)] = inner
                else:
                    values = codec.application_values(inner)
                    results[(property_identifier, array_index)] = tuple(
                        int.from_bytes(value, "big") for _, value in values[:2])
                position = end
    except (IndexError, struct.error):
        return None
    return results


# ---------------------------------------------------------------------------
# BACnetLandingCallStatus (cl. 21.6) - an Elevator Group's Landing_Call_Control / Landing_Calls
# ---------------------------------------------------------------------------

def landing_call_status(floor_number, direction=None, destination=None):
    """A BACnetLandingCallStatus: floor-number [0], then the command CHOICE direction [1]
    (a BACnetLiftCarDirection) or destination [2] (a floor)."""
    octets = context_unsigned(0, floor_number)
    if direction is not None:
        octets += context_unsigned(1, direction)
    else:
        octets += context_unsigned(2, destination)
    return octets


def landing_call_statuses(octets):
    """Every BACnetLandingCallStatus in `octets` (a property value), as
    [(floor, 'direction' or 'destination', value)]."""
    calls = []
    floor = None
    for number, body in codec.context_values(octets or b""):
        value = int.from_bytes(body, "big")
        if number == 0:
            floor = value
        elif number in (1, 2) and floor is not None:
            calls.append((floor, "direction" if number == 1 else "destination", value))
            floor = None
    return calls
