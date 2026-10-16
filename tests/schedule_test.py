#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-AEC Wire Test - the Peer sets the controller's clock, and the Schedule drives the output on it.

    python3 tests/schedule_test.py [--exe build/BACnetExampleBAEC] [--port 48040]

135-2024 L.13.1: "Ability to schedule internal values, based on date and time"
(SCHED-I-B) and "Ability to synchronize its internal clock upon request" (DM-TS-B).
Schedule 1 "Saffron" writes Analog Output 1 "Chartreuse" at priority 8: 75.0 from Monday
08:00, Schedule_Default 20.0 otherwise, 5.0 all day 2026-12-25 (an Exception_Schedule
entry). The Peer moves the controller's clock with TimeSynchronization (cl. 16.7) and
then lets it RUN: the 08:00 transition must happen on the controller's own clock, a few
seconds after the sync, not because of the sync.

  1. Control: before any sync, Local_Date is this host's date.
  2. TimeSynchronization Monday 2026-10-19 07:59:56 -> Local_Date 2026-10-19; Chartreuse
     20.0 (before the transition). Then, with no further message, within 10 s Local_Time
     passes 08:00 and Chartreuse becomes 75.0 with Priority_Array[8] = 75.0.
  3. A WriteProperty of Chartreuse 50.0 at priority 5 overrides the schedule (Present_Value
     50.0, Priority_Array[8] still 75.0); relinquishing it gives the schedule back (75.0).
  4. TimeSynchronization 2026-12-25 10:00 -> the exception: Chartreuse 5.0.
"""
import datetime
import sys
import time

import aec_setup as setup
from aec_setup import alarm_codec, wiretest
from wiretest import PROPERTY_PRESENT_VALUE, PROPERTY_PRIORITY_ARRAY, record

SETTLE = 1.5    # seconds for the controller to apply a sync and tick the schedule


def synchronize(workstation, args, moment):
    workstation.send_unconfirmed(alarm_codec.time_synchronization(
        moment.year, moment.month, moment.day, moment.isoweekday(), moment.hour, moment.minute, moment.second),
        args.port)


def local_date(workstation, args):
    octets = setup.read_octets(workstation, args, setup.DEVICE, alarm_codec.PROPERTY_LOCAL_DATE)
    if octets is None or len(octets) != 5:
        return None
    return datetime.date(1900 + octets[1], octets[2], octets[3])


def local_time(workstation, args):
    octets = setup.read_octets(workstation, args, setup.DEVICE, alarm_codec.PROPERTY_LOCAL_TIME)
    if octets is None or len(octets) != 5:
        return None
    return datetime.time(octets[1], octets[2], octets[3])


def chartreuse(workstation, args):
    return (setup.read(workstation, args, setup.CHARTREUSE, PROPERTY_PRESENT_VALUE),
            setup.read(workstation, args, setup.CHARTREUSE, PROPERTY_PRIORITY_ARRAY, setup.SCHEDULE_WRITE_PRIORITY))


def run(workstation, args):
    before = local_date(workstation, args)
    today = datetime.date.today()
    record("Control: before any sync, Local_Date is this host's date",
           before in (today, today - datetime.timedelta(days=1), today + datetime.timedelta(days=1)),
           f"read {before}, host {today}")

    # 2. The transition happens on the controller's running clock.
    just_before = datetime.datetime(2026, 10, 19, 7, 59, 56)
    synchronize(workstation, args, just_before)
    time.sleep(0.5)
    synced = local_date(workstation, args)
    clock = local_time(workstation, args)
    record("TimeSynchronization Monday 2026-10-19 07:59:56: Local_Date reads 2026-10-19 and Local_Time 07:59:5x",
           synced == just_before.date() and clock is not None and datetime.time(7, 59, 55) <= clock < datetime.time(8, 0),
           f"read {synced} {clock}")
    state = chartreuse(workstation, args)
    record("Before 08:00: Chartreuse is Schedule_Default 20.0, written at priority 8",
           state == (setup.SCHEDULE_DEFAULT_VALUE, setup.SCHEDULE_DEFAULT_VALUE),
           f"read (Present_Value, Priority_Array[8]) {state}")
    moved = wiretest.wait_until(lambda: chartreuse(workstation, args) == (setup.SCHEDULE_WEEKLY_VALUE,
                                                                         setup.SCHEDULE_WEEKLY_VALUE), 10.0, 0.5)
    clock = local_time(workstation, args)
    record("With no further message, the clock passes 08:00 and Chartreuse becomes 75.0 (Priority_Array[8] 75.0)",
           moved and clock is not None and clock >= datetime.time(8, 0), f"read {chartreuse(workstation, args)} at {clock}")

    # 3. A higher priority overrides the schedule; relinquishing hands it back.
    answer = workstation.write_property(args.port, *setup.CHARTREUSE, PROPERTY_PRESENT_VALUE,
                                        wiretest.application_real(50.0), priority=5)
    record("WriteProperty Chartreuse 50.0 at priority 5 -> SimpleACK", answer.ok, answer.describe())
    state = chartreuse(workstation, args)
    record("Priority 5 wins: Present_Value 50.0, the schedule's Priority_Array[8] still 75.0",
           state == (50.0, setup.SCHEDULE_WEEKLY_VALUE), f"read {state}")
    answer = workstation.write_property(args.port, *setup.CHARTREUSE, PROPERTY_PRESENT_VALUE,
                                        wiretest.application_null(), priority=5)
    state = chartreuse(workstation, args)
    record("Relinquishing priority 5 gives the schedule back: 75.0", answer.ok and state ==
           (setup.SCHEDULE_WEEKLY_VALUE, setup.SCHEDULE_WEEKLY_VALUE), f"{answer.describe()}, read {state}")

    # 4. The exception date.
    synchronize(workstation, args, datetime.datetime(2026, 12, 25, 10, 0, 0))
    time.sleep(SETTLE)
    state = chartreuse(workstation, args)
    record("TimeSynchronization 2026-12-25 10:00: the Exception_Schedule entry gives 5.0",
           state == (setup.SCHEDULE_EXCEPTION_VALUE, setup.SCHEDULE_EXCEPTION_VALUE), f"read {state}")
    value = setup.read(workstation, args, setup.SAFFRON, PROPERTY_PRESENT_VALUE)
    record("Saffron's own Present_Value reads 5.0", value == setup.SCHEDULE_EXCEPTION_VALUE, f"read {value}")


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    workstation = setup.RetryingWorkstation()
    try:
        run(workstation, args)
    finally:
        code = setup.finish(controller, workstation)
    return code


if __name__ == "__main__":
    sys.exit(main())
