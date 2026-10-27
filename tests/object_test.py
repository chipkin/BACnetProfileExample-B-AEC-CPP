#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-AEC Wire Test - another device creates, modifies and deletes objects (DM-OCD-B).

    python3 tests/object_test.py [--exe build/BACnetExampleBAEC] [--port 47834]

135-2024 L.13.1 Data Sharing: "Ability to allow creation, deletion, and modification of
some or all of its BACnet objects by another device". Analog Value is the one creatable
type (main.cpp, DM-OCD-B); the controller keeps a created Analog Value's Present_Value
itself (g_createdAnalogValues), up to 8 of them.

  1. Control: Object_List has no Analog Value 100; reading it gives OBJECT / UNKNOWN_OBJECT.
  2. CreateObject Analog Value 100 -> CreateObject-ACK naming it; Object_List lists it;
     Present_Value reads 0.0; WriteProperty 42.5 -> SimpleACK, reads 42.5.
  3. CreateObject by type (no instance) -> the controller picks an instance and lists it.
  4. DeleteObject Analog Value 100 -> SimpleACK; gone from Object_List; a read is
     UNKNOWN_OBJECT again.
  5. FAILURE MODES (cl. 15.3.1.3.1 / 15.4.1.3.1):
       CreateObject Analog Input 7 (a type this device does not create) -> an OBJECT Error;
       CreateObject of an Analog Value that exists -> OBJECT / OBJECT_IDENTIFIER_ALREADY_EXISTS;
       a ninth Analog Value -> RESOURCES / NO_SPACE_FOR_OBJECT;
       DeleteObject Analog Output 1 -> OBJECT / OBJECT_DELETION_NOT_PERMITTED;
       DeleteObject of an Analog Value that does not exist -> OBJECT / UNKNOWN_OBJECT.
"""
import sys

import aec_setup as setup
from aec_setup import services, wiretest
from wiretest import PROPERTY_PRESENT_VALUE, record

ANALOG_VALUE = setup.OBJECT_TYPE_ANALOG_VALUE
CREATED = (ANALOG_VALUE, 100)


def create(workstation, args, instance=None, object_type=ANALOG_VALUE):
    return workstation.request(lambda invoke_id: services.create_object_request(invoke_id, object_type, instance),
                               args.port)


def delete(workstation, args, object_type, instance):
    return workstation.request(lambda invoke_id: services.delete_object_request(invoke_id, object_type, instance),
                               args.port)


def run(workstation, args):
    objects = setup.object_list(workstation, args)
    record("Control: Object_List has no Analog Value 100", objects is not None and CREATED not in objects,
           f"{len(objects or [])} objects")
    answer = workstation.read_property(args.port, *CREATED, PROPERTY_PRESENT_VALUE)
    record("Control: reading Analog Value 100 gives OBJECT / UNKNOWN_OBJECT",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, wiretest.ERROR_CODE_UNKNOWN_OBJECT), answer.describe())

    # 2. Create by identifier, modify.
    answer = create(workstation, args, 100)
    named = services.created_object_identifier(answer)
    record("CreateObject Analog Value 100 -> CreateObject-ACK naming it", named == CREATED,
           f"ACK names {named}" if named else answer.describe())
    objects = setup.object_list(workstation, args)
    record("Object_List now lists Analog Value 100", objects is not None and CREATED in objects)
    value = setup.read(workstation, args, CREATED, PROPERTY_PRESENT_VALUE)
    record("The new object's Present_Value reads 0.0", value == 0.0, f"read {value}")
    answer = workstation.write_property(args.port, *CREATED, PROPERTY_PRESENT_VALUE, wiretest.application_real(42.5))
    record("WriteProperty Present_Value 42.5 -> SimpleACK", answer.ok, answer.describe())
    value = setup.read(workstation, args, CREATED, PROPERTY_PRESENT_VALUE)
    record("Present_Value reads 42.5", value == 42.5, f"read {value}")

    # 3. Create by type.
    answer = create(workstation, args)
    picked = services.created_object_identifier(answer)
    record("CreateObject by type -> the controller picks an Analog Value instance",
           picked is not None and picked[0] == ANALOG_VALUE and picked != CREATED,
           f"ACK names {picked}" if picked else answer.describe())
    objects = setup.object_list(workstation, args)
    record("Object_List lists the picked instance", picked is not None and objects is not None and picked in objects,
           f"picked {picked}")

    # 4. Delete.
    answer = delete(workstation, args, *CREATED)
    record("DeleteObject Analog Value 100 -> SimpleACK", answer.ok, answer.describe())
    objects = setup.object_list(workstation, args)
    record("Object_List no longer lists Analog Value 100", objects is not None and CREATED not in objects)
    answer = workstation.read_property(args.port, *CREATED, PROPERTY_PRESENT_VALUE)
    record("Reading Analog Value 100 gives UNKNOWN_OBJECT again",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, wiretest.ERROR_CODE_UNKNOWN_OBJECT), answer.describe())

    # 5. Failure Modes.
    answer = create(workstation, args, 7, wiretest.OBJECT_TYPE_ANALOG_INPUT)
    record("FAILURE MODE: CreateObject Analog Input 7 (not a creatable type) -> an OBJECT Error",
           answer.error is not None and answer.error[0] == wiretest.ERROR_CLASS_OBJECT and
           answer.error[1] in (wiretest.ERROR_CODE_DYNAMIC_CREATION_NOT_SUPPORTED,
                               wiretest.ERROR_CODE_UNSUPPORTED_OBJECT_TYPE), answer.describe())
    objects = setup.object_list(workstation, args)
    record("... and no Analog Input 7 appears", objects is not None and (wiretest.OBJECT_TYPE_ANALOG_INPUT, 7) not in objects)
    answer = create(workstation, args, picked[1] if picked else 101)
    record("FAILURE MODE: CreateObject of an Analog Value that exists -> OBJECT / OBJECT_IDENTIFIER_ALREADY_EXISTS",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, wiretest.ERROR_CODE_OBJECT_IDENTIFIER_ALREADY_EXISTS),
           answer.describe())
    created = 1 if picked else 0
    filled = True
    for instance in range(200, 200 + setup.MAX_CREATED_ANALOG_VALUES - created):
        filled = create(workstation, args, instance).ok and filled
    record(f"Creating Analog Values up to the limit of {setup.MAX_CREATED_ANALOG_VALUES} succeeds", filled)
    answer = create(workstation, args, 300)
    record(f"FAILURE MODE: Analog Value number {setup.MAX_CREATED_ANALOG_VALUES + 1} -> RESOURCES / NO_SPACE_FOR_OBJECT",
           answer.error == (wiretest.ERROR_CLASS_RESOURCES, wiretest.ERROR_CODE_NO_SPACE_FOR_OBJECT), answer.describe())
    objects = setup.object_list(workstation, args)
    record("... and no Analog Value 300 appears", objects is not None and (ANALOG_VALUE, 300) not in objects)
    answer = delete(workstation, args, *setup.CHARTREUSE)
    record("FAILURE MODE: DeleteObject Analog Output 1 -> OBJECT / OBJECT_DELETION_NOT_PERMITTED",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, wiretest.ERROR_CODE_OBJECT_DELETION_NOT_PERMITTED),
           answer.describe())
    objects = setup.object_list(workstation, args)
    record("... and Analog Output 1 is still there", objects is not None and setup.CHARTREUSE in objects)
    answer = delete(workstation, args, ANALOG_VALUE, 100)
    record("FAILURE MODE: DeleteObject of an Analog Value that does not exist -> OBJECT / UNKNOWN_OBJECT",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, wiretest.ERROR_CODE_UNKNOWN_OBJECT), answer.describe())


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    workstation = setup.workstation()
    try:
        run(workstation, args)
    finally:
        code = setup.finish(controller, workstation)
    return code


if __name__ == "__main__":
    sys.exit(main())
