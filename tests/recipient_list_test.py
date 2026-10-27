#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-AEC Wire Test - Crimson's Recipient_List is writable, and alarms go where it is written.

    python3 tests/recipient_list_test.py [--exe build/BACnetExampleBAEC] [--port 47834]

ANSI/ASHRAE 135-2024 cl. 12.21 (Notification Class, Recipient_List). The example seeds one
entry: the local subnet's broadcast address (network 0), all days, 00:00:00.00-23:59:59.99,
Process Identifier 1, unconfirmed, every transition. A read-only list would have to be the
single entry cl. 12.21 defines (local broadcast with no MAC address, Process Identifier 0),
so the list is writable: a management station can add itself, or replace the default. The
Passenger_Alarm Simulation flips on a timer that the test-only
BAEC_TEST_PASSENGER_ALARM_SECONDS=2 shortens to 2 s.

  1. Control: Recipient_List reads the seeded default entry, and nothing else.
  2. WriteProperty Recipient_List = the default entry + this workstation (127.0.0.1:<its
     port>, Process Identifier 2, confirmed): SimpleACK, and it reads back exactly as written.
  3. The passenger alarm: a ConfirmedEventNotification NORMAL -> OFFNORMAL for Lift 1 reaches
     the workstation with Process Identifier 2, and the to-normal one follows; the example
     also still sends the default entry's broadcast (its own TX log line).
  4. Failure Mode: a Recipient_List write that is not a list of BACnetDestination is refused,
     and the list is unchanged.
  5. Writing the default back is accepted and reads back unchanged.
"""
import sys

import aec_setup as setup
from aec_setup import alarm_codec as alarm, wiretest
from wiretest import record

CRIMSON = (15, 1)                     # Notification Class 1
PROPERTY_RECIPIENT_LIST = 102
DEFAULT_PROCESS_IDENTIFIER = 1        # main.cpp RECIPIENT_PROCESS_IDENTIFIER
WORKSTATION_PROCESS_IDENTIFIER = 2
ALARM_PERIOD = 2
NOTIFICATION_WAIT = ALARM_PERIOD * 3 + 4
BROADCAST_EVENT_TX = "UnconfirmedEventNotification"
# A Recipient_List address entry's MAC, as alarm.destination encodes it: [1] { network 0, 6-octet MAC }.
ADDRESS_MAC_PREFIX = b"\x1e\x21\x00\x65\x06"


def recipient_list(workstation, port):
    answer = workstation.read_property(port, *CRIMSON, PROPERTY_RECIPIENT_LIST)
    if answer.answered and answer.frame.pdu_type == wiretest.PDU_COMPLEX_ACK:
        return answer.value_octets, answer
    return None, answer


def default_entry(octets):
    """The seeded entry as alarm.destination builds it, with the MAC the device reports; or None."""
    if octets is None or ADDRESS_MAC_PREFIX not in octets:
        return None
    start = octets.index(ADDRESS_MAC_PREFIX) + len(ADDRESS_MAC_PREFIX)
    return alarm.destination(octets[start:start + 6], DEFAULT_PROCESS_IDENTIFIER, False)


def from_lift(to_state):
    return lambda notification: (notification.confirmed and notification.event_object == setup.MAUVE and
                                 notification.to_state == to_state and
                                 notification.process_identifier == WORKSTATION_PROCESS_IDENTIFIER)


def run(controller, workstation, args):
    # --- 1. Control ------------------------------------------------------------------------
    octets, answer = recipient_list(workstation, args.port)
    default = default_entry(octets)
    record("Control: Recipient_List is the seeded default only (subnet broadcast, Process Identifier 1, "
           "unconfirmed, always, every transition)",
           default is not None and octets == default, octets.hex() if octets is not None else answer.describe())
    if default is None:
        return

    # --- 2. The workstation writes itself in -----------------------------------------------
    written = default + alarm.destination(workstation.mac, WORKSTATION_PROCESS_IDENTIFIER, True)
    answer = workstation.write_property(args.port, *CRIMSON, PROPERTY_RECIPIENT_LIST, written)
    record("WriteProperty Recipient_List (default entry + this workstation, confirmed) is accepted",
           answer.acked, answer.describe())
    octets, answer = recipient_list(workstation, args.port)
    record("  ... and reads back exactly as written", octets == written,
           octets.hex() if octets is not None else answer.describe())

    # --- 3. The alarm reaches the written recipients -----------------------------------------
    broadcasts_before = controller.output().count(BROADCAST_EVENT_TX)
    mark = workstation.mark()
    offnormal = workstation.wait_for_notification(from_lift(alarm.EVENT_STATE_OFFNORMAL), NOTIFICATION_WAIT, mark)
    record("Alarm: a ConfirmedEventNotification NORMAL -> OFFNORMAL for Lift 1 reaches the written workstation, "
           "Process Identifier 2", offnormal is not None,
           offnormal.describe() if offnormal else
           f"got {[received.describe() for received in workstation.notifications_since(mark)]}")
    normal = workstation.wait_for_notification(from_lift(alarm.EVENT_STATE_NORMAL), NOTIFICATION_WAIT, mark)
    record("Released: the to-normal ConfirmedEventNotification reaches the written workstation",
           normal is not None and normal.from_state == alarm.EVENT_STATE_OFFNORMAL,
           normal.describe() if normal else
           f"got {[received.describe() for received in workstation.notifications_since(mark)]}")
    broadcasts = controller.output().count(BROADCAST_EVENT_TX) - broadcasts_before
    record("The default entry still gets its UnconfirmedEventNotification broadcast (example TX log)",
           broadcasts > 0, f"{broadcasts} UnconfirmedEventNotification TX line(s)")

    # --- 4. Failure Mode ----------------------------------------------------------------------
    answer = workstation.write_property(args.port, *CRIMSON, PROPERTY_RECIPIENT_LIST,
                                        wiretest.application_unsigned(5))
    record("FAILURE MODE: a Recipient_List write that is not a list of BACnetDestination is refused",
           answer.answered and answer.frame.pdu_type in (wiretest.PDU_ERROR, wiretest.PDU_REJECT),
           answer.describe())
    octets, answer = recipient_list(workstation, args.port)
    record("  ... and the list is unchanged", octets == written,
           octets.hex() if octets is not None else answer.describe())

    # --- 5. The default back --------------------------------------------------------------------
    answer = workstation.write_property(args.port, *CRIMSON, PROPERTY_RECIPIENT_LIST, default)
    record("WriteProperty Recipient_List back to the default entry is accepted", answer.acked, answer.describe())
    octets, answer = recipient_list(workstation, args.port)
    record("  ... and reads back unchanged", octets == default,
           octets.hex() if octets is not None else answer.describe())


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args, environment={"BAEC_TEST_PASSENGER_ALARM_SECONDS": ALARM_PERIOD})
    workstation = setup.workstation()
    try:
        run(controller, workstation, args)
    finally:
        result = setup.finish(controller, workstation)
    return result


if __name__ == "__main__":
    sys.exit(main())
