#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-AEC Wire Test - a landing call commands the elevator, and the car answers it.

    python3 tests/elevator_test.py [--exe build/BACnetExampleBAEC] [--port 48040]

135-2024 L.13.1: "A B-AEC device performs control of elevators. It supports the
modification and commanding of its elevator objects by another device." The Peer writes
Elevator Group 1 "Maroon"'s Landing_Call_Control - cl. 12.58.10: "A write to this
property is equivalent to a passenger pressing a call button at a landing" - and watches
Lift 1 "Mauve". The car behind Mauve is the example's Simulation (main.cpp, "SIMULATION:
the elevator car"): one floor every 2 s, door open 3 s at each stop.

  1. Control: idle at floor 3, STOPPED, door CLOSED, no landing calls.
  2. Landing call floor 6 UP -> SimpleACK; Landing_Calls and Assigned_Landing_Calls list
     it; the car moves UP through floors 4 and 5 (Car_Position never skips or goes back),
     stops at 6 with the door OPENED, the call leaves both lists, the door CLOSES.
  3. Landing call floor 2 DOWN -> the car moves DOWN to 2.
  4. FAILURE MODE: a landing call for floor 11 (this installation has floors 1..10), and
     one for floor 0 -> an Error, and the car does not move; Landing_Calls stays empty.
"""
import sys
import time

import aec_setup as setup
from aec_setup import services, wiretest
from wiretest import record, known_gap

POLL = 0.2


CAR_PROPERTIES = [(setup.PROPERTY_CAR_POSITION, None), (setup.PROPERTY_CAR_MOVING_DIRECTION, None),
                  (setup.PROPERTY_CAR_DOOR_STATUS, 1)]


def car_state(workstation, args):
    """(Car_Position, Car_Moving_Direction, Car_Door_Status[1]) in ONE ReadPropertyMultiple
    (DS-RPM-B), so the three are one snapshot of the moving car."""
    answer = workstation.request(lambda invoke_id: services.read_property_multiple_request(
        invoke_id, *setup.MAUVE, CAR_PROPERTIES), args.port)
    results = services.parse_read_property_multiple_ack(answer)
    if results is None:
        print(f"      ReadPropertyMultiple Lift 1: {answer.describe()}")
        return (None, None, None)
    return tuple(wiretest.decode_application(results.get(reference)) if isinstance(results.get(reference), bytes)
                 else None for reference in CAR_PROPERTIES)


def landing_calls(workstation, args):
    return services.landing_call_statuses(
        setup.read_octets(workstation, args, setup.MAROON, setup.PROPERTY_LANDING_CALLS))


def assigned_calls(workstation, args):
    octets = setup.read_octets(workstation, args, setup.MAUVE, setup.PROPERTY_ASSIGNED_LANDING_CALLS, 1)
    return [(floor, value) for floor, _, value in services.landing_call_statuses(octets)] if octets is not None else None


def call(workstation, args, floor, direction):
    return workstation.write_property(args.port, *setup.MAROON, setup.PROPERTY_LANDING_CALL_CONTROL,
                                      services.landing_call_status(floor, direction=direction))


def watch_trip(workstation, args, seconds):
    """Poll the car for `seconds`; returns [(position, direction, door)] samples."""
    samples = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        samples.append(car_state(workstation, args))
        time.sleep(POLL)
    return samples


def positions_in_order(samples):
    """The distinct Car_Positions seen, in the order seen."""
    seen = []
    for position, _, _ in samples:
        if position is not None and (not seen or seen[-1] != position):
            seen.append(position)
    return seen


def run(workstation, args):
    state = car_state(workstation, args)
    record("Control: idle at floor 3, STOPPED, door CLOSED",
           state == (setup.LIFT_START_FLOOR, setup.DIRECTION_STOPPED, setup.DOOR_CLOSED),
           f"read (Car_Position, Car_Moving_Direction, Car_Door_Status[1]) {state}")
    calls = landing_calls(workstation, args)
    record("Control: Landing_Calls is empty", calls == [], f"read {calls}")

    # 2. Up to floor 6.
    answer = call(workstation, args, 6, setup.DIRECTION_UP)
    record("Landing call floor 6 UP is acknowledged", answer.ok, answer.describe())
    calls = landing_calls(workstation, args)
    record("Landing_Calls lists floor 6 UP", calls == [(6, "direction", setup.DIRECTION_UP)], f"read {calls}")
    assigned = assigned_calls(workstation, args)
    record("Lift 1 Assigned_Landing_Calls[1] lists floor 6 UP", assigned == [(6, setup.DIRECTION_UP)],
           f"read {assigned}")
    samples = watch_trip(workstation, args, 3 * setup.LIFT_SECONDS_PER_FLOOR + 1.5)
    visited = positions_in_order(samples)
    record("The car travels 3 -> 4 -> 5 -> 6, floor by floor", visited == [3, 4, 5, 6], f"positions seen {visited}")
    moving_up = [direction for position, direction, _ in samples if position in (4, 5)]
    record("Between floors 3 and 6, Car_Moving_Direction is UP",
           moving_up != [] and all(direction == setup.DIRECTION_UP for direction in moving_up),
           f"directions at floors 4-5 {sorted(set(moving_up))}")
    state = car_state(workstation, args)
    record("At floor 6: STOPPED, door OPENED", state == (6, setup.DIRECTION_STOPPED, setup.DOOR_OPENED),
           f"read {state}")
    calls = landing_calls(workstation, args)
    assigned = assigned_calls(workstation, args)
    record("The call is answered: Landing_Calls and Assigned_Landing_Calls are empty",
           calls == [] and assigned == [], f"read {calls} / {assigned}")
    closed = wiretest.wait_until(lambda: car_state(workstation, args) == (6, setup.DIRECTION_STOPPED, setup.DOOR_CLOSED),
                                 setup.LIFT_DOOR_DWELL_SECONDS + 1.5)
    record("The door closes after its dwell", closed, f"read {car_state(workstation, args)}")
    control = services.landing_call_statuses(
        setup.read_octets(workstation, args, setup.MAROON, setup.PROPERTY_LANDING_CALL_CONTROL))
    record("Landing_Call_Control reads back the last call written", control == [(6, "direction", setup.DIRECTION_UP)],
           f"read {control}")

    # 3. Down to floor 2.
    answer = call(workstation, args, 2, setup.DIRECTION_DOWN)
    record("Landing call floor 2 DOWN is acknowledged", answer.ok, answer.describe())
    samples = watch_trip(workstation, args, 4 * setup.LIFT_SECONDS_PER_FLOOR + 1.5)
    visited = positions_in_order(samples)
    record("The car travels 6 -> 5 -> 4 -> 3 -> 2", visited == [6, 5, 4, 3, 2], f"positions seen {visited}")
    moving_down = [direction for position, direction, _ in samples if position in (5, 4, 3)]
    record("Between floors 6 and 2, Car_Moving_Direction is DOWN",
           moving_down != [] and all(direction == setup.DIRECTION_DOWN for direction in moving_down),
           f"directions at floors 3-5 {sorted(set(moving_down))}")
    wiretest.wait_until(lambda: car_state(workstation, args)[2] == setup.DOOR_CLOSED, setup.LIFT_DOOR_DWELL_SECONDS + 1.5)

    # 4. FAILURE MODE: floors this installation does not have.
    before = car_state(workstation, args)
    for floor in (setup.LIFT_HIGHEST_FLOOR + 1, 0):
        answer = call(workstation, args, floor, setup.DIRECTION_UP)
        record(f"FAILURE MODE: a landing call for floor {floor} is refused with an Error",
               answer.answered and answer.error is not None, answer.describe())
        known_gap(f"The refusal of floor {floor} is PROPERTY / VALUE_OUT_OF_RANGE (cl. 15.9.1.3.1)",
                  answer.error == (services.ERROR_CLASS_PROPERTY, services.ERROR_CODE_VALUE_OUT_OF_RANGE),
                  "stack: CallbackSetElevatorGroupLandingCallControl has no error-code out-parameter, "
                  f"so the stack picks the error (got {answer.describe()}) - TODO.md")
    samples = watch_trip(workstation, args, 2 * setup.LIFT_SECONDS_PER_FLOOR)
    record("After the refused calls the car does not move",
           all(sample == before for sample in samples), f"before {before}, seen {sorted(set(samples))}")
    calls = landing_calls(workstation, args)
    record("Landing_Calls stays empty", calls == [], f"read {calls}")


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
