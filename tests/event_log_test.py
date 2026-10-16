#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-AEC Wire Test - the Event Log captures the elevator's own alarms, read back with ReadRange.

    python3 tests/event_log_test.py [--exe build/BACnetExampleBAEC] [--port 48040]

135-2024 L.13.1: "Logging of event notifications of the local device in an Event Log
object" (AE-EL-I-B). Lift 1 "Mauve" alarms on its Passenger_Alarm (an intrinsic
CHANGE_OF_STATE algorithm, Notification Class 1); the alarm is Simulation, flipping on a
timer that the test-only BAEC_TEST_PASSENGER_ALARM_SECONDS=2 shortens to 2 s. Event Log 1
"Beige"'s Log_Buffer can only be read with ReadRange (cl. 12.27.13).

  1. Beige's Enable at start-up - TRUE is what L.13.1 asks for; the stack starts it FALSE
     and has no customer setter (stack #3301): a [KNOWN GAP].
  2. Control: with Enable FALSE, a Passenger_Alarm transition logs nothing (Record_Count 0).
  3. The Peer writes Enable TRUE -> SimpleACK. The next transition is logged: Record_Count
     grows, and ReadRange of Log_Buffer returns Record_Count records, among them an event
     notification from Device 389014 for Lift 1 whose to-state matches Passenger_Alarm.
  4. ReadRange by position (index 1, count 1) returns exactly one record.
  5. Each transition is logged once - the device logs its own broadcast notification a
     second time when it hears it back (stack #3957): a [KNOWN GAP].
  6. The Peer writes Enable FALSE: the next transition is not logged.
"""
import sys
import time

import aec_setup as setup
from aec_setup import alarm_codec, services, wiretest
from wiretest import known_gap, record

ALARM_PERIOD = 2
TRANSITION_WAIT = ALARM_PERIOD * 2 + 2


def passenger_alarm(workstation, args):
    return setup.read(workstation, args, setup.MAUVE, setup.PROPERTY_PASSENGER_ALARM)


def wait_for_transition(workstation, args):
    """Wait until Passenger_Alarm changes; returns its new value (None if it never did)."""
    start = passenger_alarm(workstation, args)
    changed = wiretest.wait_until(lambda: passenger_alarm(workstation, args) not in (start, None), TRANSITION_WAIT)
    if not changed:
        return None
    time.sleep(0.5)    # let the stack evaluate the algorithm and log
    return passenger_alarm(workstation, args)


def record_count(workstation, args):
    return setup.read(workstation, args, setup.BEIGE, setup.PROPERTY_RECORD_COUNT)


def read_range(workstation, args, reference_index=None, count=None):
    answer = workstation.request(lambda invoke_id: services.read_range_request(
        invoke_id, *setup.BEIGE, setup.PROPERTY_LOG_BUFFER, reference_index, count), args.port)
    return services.parse_read_range_ack(answer), answer


def lift_notifications(records):
    return [entry for entry in records if entry["kind"] == "notification" and entry["event_object"] == setup.MAUVE]


def run(workstation, args):
    enable = setup.read(workstation, args, setup.BEIGE, setup.PROPERTY_ENABLE)
    known_gap("Event Log 1 is enabled at start-up, logging without an operator's write first",
              enable is True, f"stack #3301: Enable starts FALSE and the customer API cannot set it (read {enable})")

    # 2. Control: disabled logs nothing.
    if enable is not True:
        count = record_count(workstation, args)
        record("Control: Record_Count starts at 0", count == 0, f"read {count}")
        state = wait_for_transition(workstation, args)
        record("Control: a Passenger_Alarm transition happens", state is not None, f"Passenger_Alarm now {state}")
        count = record_count(workstation, args)
        record("Control: with Enable FALSE the transition logs nothing (Record_Count 0)", count == 0, f"read {count}")

    # 3. Enable, and the next transition is logged.
    answer = workstation.write_property(args.port, *setup.BEIGE, setup.PROPERTY_ENABLE,
                                        wiretest.application_boolean(True))
    record("WriteProperty Enable TRUE -> SimpleACK", answer.ok, answer.describe())
    enabled_count = record_count(workstation, args)
    state = wait_for_transition(workstation, args)
    record("A Passenger_Alarm transition happens", state is not None, f"Passenger_Alarm now {state}")
    count = wiretest.wait_until(lambda: (record_count(workstation, args) or 0) > (enabled_count or 0) and
                                record_count(workstation, args), 2.0)
    record("Record_Count grows after the transition", count and count > (enabled_count or 0),
           f"{enabled_count} -> {count}")
    log, answer = read_range(workstation, args)
    record("ReadRange of Log_Buffer -> ReadRange-ACK", log is not None,
           f"item count {log['item_count']}" if log else answer.describe())
    records = services.event_log_records(log["item_data"]) if log else []
    count = record_count(workstation, args)
    record("ReadRange returns Record_Count records", log is not None and log["item_count"] == count == len(records),
           f"item count {log and log['item_count']}, decoded {len(records)}, Record_Count {count}")
    expected_state = alarm_codec.EVENT_STATE_OFFNORMAL if state else alarm_codec.EVENT_STATE_NORMAL
    logged = [entry for entry in lift_notifications(records) if entry["to_state"] == expected_state]
    record("The log holds the event notification: Device 389014, Lift 1, to-state matching Passenger_Alarm",
           any(entry["initiating_device"] == setup.DEVICE for entry in logged),
           f"to-state {expected_state}; records {[(entry['kind'], entry.get('event_object'), entry.get('to_state')) for entry in records]}")

    # 4. By position.
    first, answer = read_range(workstation, args, 1, 1)
    first_records = services.event_log_records(first["item_data"]) if first else []
    record("ReadRange by position (1, 1) returns exactly one record",
           first is not None and first["item_count"] == 1 and len(first_records) == 1,
           f"item count {first['item_count']}, first record {first_records[0]['kind'] if first_records else None}"
           if first else answer.describe())

    # 5. Logged once per transition.
    stamps = [entry["notification"] for entry in logged]
    known_gap("Each transition is logged once, not again when the device hears its own broadcast",
              len(stamps) == 1, f"stack #3957: {len(stamps)} records of the one transition "
              f"(process identifiers {[entry['process_identifier'] for entry in logged]})")

    # 6. Disable: the next transition is not logged.
    answer = workstation.write_property(args.port, *setup.BEIGE, setup.PROPERTY_ENABLE,
                                        wiretest.application_boolean(False))
    record("WriteProperty Enable FALSE -> SimpleACK", answer.ok, answer.describe())
    time.sleep(0.5)
    disabled_count = record_count(workstation, args)
    state = wait_for_transition(workstation, args)
    count = record_count(workstation, args)
    record("With Enable FALSE again, the next transition is not logged", state is not None and count == disabled_count,
           f"Passenger_Alarm now {state}; Record_Count {disabled_count} -> {count}")


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args, environment={"BAEC_TEST_PASSENGER_ALARM_SECONDS": ALARM_PERIOD})
    workstation = setup.workstation()
    try:
        run(workstation, args)
    finally:
        code = setup.finish(controller, workstation)
    return code


if __name__ == "__main__":
    sys.exit(main())
