# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""What B-AEC's Profile Function Wire Tests share: this controller's numbers, and starting it.

The setup, all on one host:

    Workstation (127.0.0.1, ephemeral UDP port) - the Peer: a Python operator workstation
        |  BACnet/IP, unicast both ways
    BACnetExampleBAEC   UDP --port   Device 389014

The Peer plays the building's operator workstation or elevator management station: it
places landing calls (L.13.1: "commanding of its elevator objects by another device"),
creates and deletes objects, synchronises the controller's clock, turns the Event Log on
and reads it back with ReadRange, and backs the controller's configuration up and
restores it. Nothing reaches into the example any other way; the one test-only switch is
the BAEC_TEST_PASSENGER_ALARM_SECONDS environment variable (main.cpp), which only makes
the simulated Passenger_Alarm flip sooner.
"""
import argparse
import sys
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPOSITORY = TESTS.parent
sys.path.insert(0, str(REPOSITORY))
sys.path.insert(0, str(TESTS))

import wiretest  # noqa: E402  (the vendored harness at the repository root)
from wiretest import Workstation  # noqa: E402,F401  (wiretest 0.6.0)
from wiretest import alarm as alarm_codec  # noqa: E402,F401
from wiretest import services  # noqa: E402,F401  (wiretest 0.7.0: CreateObject, files, ReadRange, RPM, ...)

# Match main.cpp.
DEVICE_INSTANCE = 389014
DEVICE = (wiretest.OBJECT_TYPE_DEVICE, DEVICE_INSTANCE)
OBJECT_TYPE_ANALOG_VALUE = 2
OBJECT_TYPE_FILE = 10
OBJECT_TYPE_SCHEDULE = 17
OBJECT_TYPE_EVENT_LOG = 25
OBJECT_TYPE_ELEVATOR_GROUP = 57
OBJECT_TYPE_LIFT = 59
CHARTREUSE = (wiretest.OBJECT_TYPE_ANALOG_OUTPUT, 1)   # Analog Output 1, Saffron's target
SAFFRON = (OBJECT_TYPE_SCHEDULE, 1)                    # Schedule 1
BEIGE = (OBJECT_TYPE_EVENT_LOG, 1)                     # Event Log 1
IVORY = (OBJECT_TYPE_FILE, 1)                          # File 1, the backup image
MAROON = (OBJECT_TYPE_ELEVATOR_GROUP, 1)               # Elevator Group 1
MAUVE = (OBJECT_TYPE_LIFT, 1)                          # Lift 1
BRONZE = (wiretest.OBJECT_TYPE_ANALOG_INPUT, 1)        # Analog Input 1 - a type that is NOT creatable

SCHEDULE_WRITE_PRIORITY = 8
SCHEDULE_DEFAULT_VALUE = 20.0
SCHEDULE_WEEKLY_VALUE = 75.0          # Monday 08:00 onwards
SCHEDULE_EXCEPTION_VALUE = 5.0        # all day 2026-12-25

LIFT_LOWEST_FLOOR = 1
LIFT_HIGHEST_FLOOR = 10
LIFT_START_FLOOR = 3
LIFT_SECONDS_PER_FLOOR = 2.0
LIFT_DOOR_DWELL_SECONDS = 3.0
MAX_CREATED_ANALOG_VALUES = 8

PASSWORD = "Saffron-42"               # the --dcc-password the backup test starts the controller with

# Properties.
PROPERTY_ENABLE = 133
PROPERTY_LOG_BUFFER = 131
PROPERTY_RECORD_COUNT = 141
PROPERTY_TOTAL_RECORD_COUNT = 145
PROPERTY_SYSTEM_STATUS = 112
PROPERTY_LAST_RESTORE_TIME = 157
PROPERTY_BACKUP_AND_RESTORE_STATE = 338
PROPERTY_CAR_POSITION = 458
PROPERTY_CAR_MOVING_DIRECTION = 457
PROPERTY_CAR_DOOR_STATUS = 450
PROPERTY_ASSIGNED_LANDING_CALLS = 447
PROPERTY_LANDING_CALLS = 470
PROPERTY_LANDING_CALL_CONTROL = 471
PROPERTY_PASSENGER_ALARM = 478

# BACnetLiftCarDirection, BACnetDoorStatus (cl. 21).
DIRECTION_NONE = 1
DIRECTION_STOPPED = 2
DIRECTION_UP = 3
DIRECTION_DOWN = 4
DOOR_CLOSED = 0
DOOR_OPENED = 1


def workstation():
    """The operator workstation: a wiretest.Workstation whose confirmed requests are sent again,
    with the SAME invoke id, when unanswered after 0.6 s, up to 3 times, each with a visible
    [RETRY] line (wiretest 0.7.0 Workstation(retries=, timeout=)). On a busy Windows host a
    request is now and then lost before it reaches the example (its console logs every request
    it received and an answer to each), which is what this covers."""
    return Workstation(retries=3, timeout=0.6)


def parser(description):
    arguments = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBAEC")),
                           help="path to BACnetExampleBAEC (default: build/BACnetExampleBAEC)")
    arguments.add_argument("--port", type=int, default=48040, help="the controller's UDP port (default 48040)")
    return arguments


def start_controller(args, extra_arguments=(), environment=None):
    return wiretest.start(args.exe, ["--port", args.port] + list(extra_arguments), "controller", environment)


def read(workstation, args, object_reference, property_identifier, array_index=None):
    """The decoded value of one property, or None (the Answer is printed on failure)."""
    answer = workstation.read_property(args.port, *object_reference, property_identifier, array_index)
    if not answer.ok:
        print(f"      read {object_reference} property {property_identifier}: {answer.describe()}")
    return answer.value


def read_octets(workstation, args, object_reference, property_identifier, array_index=None):
    answer = workstation.read_property(args.port, *object_reference, property_identifier, array_index)
    if not answer.ok:
        print(f"      read {object_reference} property {property_identifier}: {answer.describe()}")
    return answer.value_octets


def object_list(workstation, args):
    octets = read_octets(workstation, args, DEVICE, wiretest.PROPERTY_OBJECT_LIST)
    return wiretest.decoded_values(octets) if octets is not None else None


def finish(controller, workstation):
    workstation.close()
    controller.stop()
    if not all(wiretest.results.RESULTS):
        controller.keep_log()
    return wiretest.summary()
