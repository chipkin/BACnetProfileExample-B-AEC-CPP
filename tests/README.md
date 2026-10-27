# B-AEC Wire Tests

Tests that drive the running advanced elevator controller over BACnet/IP from
outside it. They are written against the series' shared harness in
[`../wiretest/`](../wiretest/README.md) (vendored, like `common/`, at 0.7.0);
everything specific to this device is here. Python 3 standard library only -
nothing to `pip install`.

They prove the B-AEC Profile Function. ANSI/ASHRAE 135-2024 Annex L.13.1: "A
B-AEC device performs control of elevators. It supports the modification and
commanding of its elevator objects by another device." - and, from the same
clause, "creation, deletion, and modification" of its objects by another
device, "Logging of event notifications of the local device in an Event Log
object", "Ability to schedule internal values, based on date and time",
"Ability to synchronize its internal clock upon request", and "Ability to
upload its configuration and allow it to be subsequently restored".

## Running

Build the example (README "Build"), then from the repository root:

```bash
python3 tests/run_wire_tests.py --exe build/BACnetExampleBAEC
```

On Windows, `--exe build\Release\BACnetExampleBAEC.exe`. It ends with
`ALL WIRE TESTS PASSED` (exit 0) or the scripts that failed. `--port` (default
47834) is the UDP port each script starts the controller on, one after another.
A full run takes about two and a half minutes. Each script also runs on its
own, with the same `--exe` and `--port`. CI (`.github/workflows/release.yml`)
runs the suite on Linux after the smoke test.

## The setup

```
Workstation (127.0.0.1, ephemeral UDP port)  - the Peer
    |  BACnet/IP, unicast both ways
BACnetExampleBAEC  UDP --port                - Device 389014
```

The Peer is a Python BACnet client (`wiretest.Workstation`, with a visible
retry - see below), never another example's binary. It plays the elevator
management station or operator workstation of a real site. Nothing reaches
into the example any other way. The one test-only switch is the environment
variable `BAEC_TEST_PASSENGER_ALARM_SECONDS`, which `event_log_test.py` and
`recipient_list_test.py` set to
2 so the simulated Passenger_Alarm flips every 2 s instead of 30 s; it is not a
command-line option and changes nothing a BACnet client can reach.
`backup_restore_test.py` starts the controller with `--dcc-password`.

`aec_setup.py` holds this controller's numbers and the start-up. The service
encoders and decoders (CreateObject, DeleteObject, AtomicReadFile/WriteFile,
ReadRange with Event Log records, ReadPropertyMultiple, BACnetLandingCallStatus)
are `wiretest.services` (0.7.0; they began here as `aec_services.py`).

**`[RETRY]` lines.** `aec_setup.workstation()` - `wiretest.Workstation(retries=3,
timeout=0.6)`, which began here as `RetryingWorkstation` - sends an unanswered confirmed
request again after 0.6 s with the same invoke id, up to 3 times, and prints a
`[RETRY]` line each time. On Windows, while the machine was busy with parallel
builds, about one request in a hundred went unanswered; the controller's console
logged an `RX` and a `TX` for every request it received, and a 300-request
probe on an idle moment lost none - the datagrams were delayed or lost on the
host, not refused by the example. Linux runs needed no retry.

## `discovery_test.py` - every listed property reads as a value

1. **Who-Is** (for 389014, unicast) -> the device's I-Am.
2. **Device `Object_List`** names exactly the seventeen objects `main.cpp`
   creates at start-up. This is the control: a walk over nothing would pass.
3. For each object, **`Property_List`** reads, and a ReadProperty of every
   property it lists - plus `Object_Identifier`, `Object_Name`, `Object_Type`
   and `Property_List`, which cl. 12.1.1.4.1 leaves out of the list - returns a
   ComplexACK. An Error, Reject, Abort or no answer prints an `[UNREADABLE]` line
   and fails the check.
4. **One exception: Event Log 1's `Log_Buffer` (131)** must be *refused*
   (cl. 12.27.13: "The log buffer is not network accessible except through the
   use of the ReadRange service"; ReadProperty's error table, cl. 15.5.1.3.1,
   gives PROPERTY / READ_ACCESS_DENIED).
5. Network Port 1's `IP_DNS_Server` (406), `FD_BBMD_Address` (418) and
   `FD_Subscription_Lifetime` (419) are each listed and read as a value.
6. The Device's **`Database_Revision`** reads 16, its start-up value (it goes up
   only on CreateObject / DeleteObject, which this script does not send).

## `configuration_test.py` - the Device's identity is configurable at start-up

Cl. 12.1.1.3 and 12.1.1.4 require the Device's Object_Identifier and Object_Name
to be configurable (Object_Name must be unique across the internetwork).

1. **No options** (the control): Device 389014 answers Who-Is, and its
   Object_Name is the default `Chipkin Example B-AEC`.
2. **`--deviceID 389914 --deviceName "Chipkin Example B-AEC Unit 2"`**: Who-Is,
   ReadProperty, the ready line and a Who-Has by name all see the configured
   instance and name; 389014 answers OBJECT / UNKNOWN_OBJECT. A WriteProperty of
   the Device's Object_Name is refused and the name is unchanged: it changes only
   at start-up.
3. **Limits**: an empty or a 256-byte `--deviceName` prints a warning and keeps
   the default; a 255-byte one is served whole.
4. **`--help`** lists `--port`, `--deviceID`, `--deviceName`, `--dcc-password`
   and `--xml`, the options the example reads; with `--xml` the console prints
   each frame as an XML block.

## `elevator_test.py` - a landing call commands the car

Cl. 12.58.10: a write to `Landing_Call_Control` "is equivalent to a passenger
pressing a call button at a landing". The car behind Lift 1 is the example's
Simulation (README "What is Simulation"): one floor every 2 s, door open 3 s.
Car samples are one ReadPropertyMultiple of `Car_Position`,
`Car_Moving_Direction` and `Car_Door_Status[1]`, so each is a snapshot.

1. Control: idle at floor 3, `STOPPED`, door `CLOSED`; `Landing_Calls` empty.
2. Landing call floor 6 UP -> SimpleACK; `Landing_Calls` and Lift 1's
   `Assigned_Landing_Calls[1]` list it; the car is seen at 3, 4, 5, 6 in that
   order, `UP` in between, then `STOPPED` with the door `OPENED`; the call has
   left both lists; the door closes; `Landing_Call_Control` reads back the call.
3. Landing call floor 2 DOWN -> seen at 6, 5, 4, 3, 2, `DOWN` in between.
4. **Failure Mode**: landing calls for floor 11 and floor 0 (the installation
   has floors 1-10) are refused with an Error, the car does not move, and
   `Landing_Calls` stays empty. `[KNOWN GAP]`: the Error is WRITE_ACCESS_DENIED,
   not VALUE_OUT_OF_RANGE - the callback cannot name its error (TODO.md item 4).

## `object_test.py` - CreateObject / DeleteObject (DM-OCD-B)

1. Control: no Analog Value 100 in `Object_List`; reading it is OBJECT /
   UNKNOWN_OBJECT.
2. CreateObject Analog Value 100 -> an ACK naming it; listed; `Present_Value`
   0.0; written 42.5 and read back.
3. CreateObject by type -> the controller picks an instance and lists it.
4. DeleteObject Analog Value 100 -> SimpleACK; gone; UNKNOWN_OBJECT again.
5. **Failure Modes** (cl. 15.3.1.3.1 / 15.4.1.3.1): CreateObject Analog Input 7
   -> OBJECT / DYNAMIC_CREATION_NOT_SUPPORTED (or UNSUPPORTED_OBJECT_TYPE); a
   duplicate -> OBJECT_IDENTIFIER_ALREADY_EXISTS; the ninth Analog Value ->
   RESOURCES / NO_SPACE_FOR_OBJECT; DeleteObject Analog Output 1 ->
   OBJECT_DELETION_NOT_PERMITTED; DeleteObject of a missing Analog Value ->
   UNKNOWN_OBJECT. Each refusal leaves `Object_List` as it was.

## `schedule_test.py` - the Schedule runs on the clock the Peer sets

1. Control: before any sync, `Local_Date` is the host's date.
2. TimeSynchronization Monday 2026-10-19 07:59:56 -> `Local_Date` / `Local_Time`
   follow; Analog Output 1 is `Schedule_Default` 20.0 at priority 8; then, with
   no further message, within 10 s the controller's clock passes 08:00 and the
   output is 75.0 with `Priority_Array[8]` 75.0. Before 1.2.0 the device clock
   stood still after a sync, so this check would never pass.
3. Priority 5 overrides the schedule (50.0 while `Priority_Array[8]` stays
   75.0); relinquishing it hands the output back.
4. TimeSynchronization 2026-12-25 10:00 -> the Exception_Schedule entry: 5.0.

## `event_log_test.py` - the Event Log captures the elevator's alarms

1. `[KNOWN GAP]`: Event Log 1 reads `Enable` FALSE at start-up (stack #3301).
2. Control: with `Enable` FALSE a Passenger_Alarm transition logs nothing.
3. WriteProperty `Enable` TRUE; the next transition is logged; ReadRange of
   `Log_Buffer` returns `Record_Count` records, among them an event
   notification from Device 389014 for Lift 1 whose to-state matches the new
   Passenger_Alarm.
4. ReadRange by position (1, 1) returns exactly one record.
5. `[KNOWN GAP]`: each transition is logged twice (stack #3957).
6. `Enable` FALSE again: the next transition is not logged.

## `backup_restore_test.py` - backup and restore (DM-BR-B)

The configuration is the set of created Analog Values and their values
(main.cpp, "THE CONFIGURATION IMAGE"); cl. 19.1 is the procedure.

1. Setup: Analog Value 100 = 42.5, 101 = 1.0.
2. **Failure Mode**: START_BACKUP with a wrong password -> SECURITY /
   PASSWORD_FAILURE; `System_Status` stays OPERATIONAL.
3. START_BACKUP -> `System_Status` BACKUP_IN_PROGRESS; `Configuration_Files`
   names File 1; AtomicReadFile returns the image (`B-AEC configuration 1`,
   `analog-value 100 42.5`, `analog-value 101 1`); END_BACKUP -> OPERATIONAL.
4. The change: 100 <- 7.0, 101 deleted, 102 created.
5. START_RESTORE; File 1's `File_Size` written to 0 -> SimpleACK, reads 0
   (K.5.18); **Failure Mode**: a `File_Size` over 4096 -> PROPERTY /
   VALUE_OUT_OF_RANGE; AtomicWriteFile of the saved image, `File_Size` reads
   its length; control: 100 still 7.0 before END_RESTORE; END_RESTORE.
6. 100 is 42.5 again, 101 is back with 1.0, 102 is gone; `Last_Restore_Time`
   moved.
7. **Failure Mode**: a restore of a file that is not an image changes nothing,
   and `Last_Restore_Time` does not move.

## `recipient_list_test.py` - the Recipient_List is writable

Notification Class 1 "Crimson" (cl. 12.21). A read-only Recipient_List would have
to be the single local-broadcast entry with Process Identifier 0; this example
seeds the subnet broadcast with Process Identifier 1, so the list is writable.

1. Control: `Recipient_List` reads the default entry only (the local subnet
   broadcast, Process Identifier 1, unconfirmed, always, every transition).
2. WriteProperty of the default entry + this workstation (Process Identifier 2,
   confirmed) -> SimpleACK; it reads back exactly as written.
3. The next Passenger_Alarm transitions reach the workstation through the
   written entry: a ConfirmedEventNotification NORMAL -> OFFNORMAL for Lift 1,
   then the return to normal; the default broadcast is still sent (the
   example's TX log).
4. **Failure Mode**: a Recipient_List write that is not a list of
   BACnetDestination is refused, and the list is unchanged.
5. Writing the default back is accepted and reads back unchanged.

## Results

Run 2026-10-16 at stack `6.x` @ `64b2cfac5`, example 1.2.0. 107 checks per run
(24 + 18 + 21 + 9 + 12 + 23), 4 `[KNOWN GAP]` lines.

| Platform | Build | Runs | Result | `[RETRY]` lines |
|---|---|---|---|---|
| Windows 11 | STATIC (MSVC, NMake) | 3 in a row | 107/107 each | 6, 17, 16 |
| Linux (WSL Ubuntu 22.04) | SOURCE (GCC) | 3 in a row | 107/107 each | 0, 0, 0 |

## Mutation verification

Each feature was removed from `main.cpp` on its own, the Windows STATIC build
relinked (exe time checked), and the test that covers it was run. Every one
failed; the failing lines are in the 1.2.0 commit message.

| # | Removed from `main.cpp` | Test | Result |
|---|---|---|---|
| M1 | `LiftCarTick()` never called (the car Simulation stops) | elevator | 10/18 - the car never leaves floor 3 |
| M2 | A landing call is acknowledged but not queued | elevator | 10/18 - `Landing_Calls` empty, the car never moves |
| M3 | The floor range check | elevator | 14/18 - floors 11 and 0 SimpleACKed, the car moves |
| M4 | TimeSynchronization does not set the clock | schedule | 3/9 - `Local_Date` stays the host's |
| M5 | The clock freezes at the synchronised moment (the 1.1.0 behaviour) | schedule | 6/9 - 08:00 never comes, Chartreuse stays 20.0 |
| M6 | CreateObject callback refuses everything | object | 11/21 - RESOURCES / NO_SPACE_FOR_OBJECT for every create |
| M7 | DeleteObject does not release the slot | object | 20/21 - the limit is reached one object early |
| M8 | A backup writes no configuration image | backup_restore | 15/23 - File 1 empty, nothing restored |
| M9 | A restore is parsed but never applied | backup_restore | 18/23 - Analog Value 100 stays 7.0 |
| M10 | A restore applies whatever was written, image or not | backup_restore | 21/23 - the bad image undoes a later change; `Last_Restore_Time` moves |
| M11 | ReinitializeDevice ignores the password | backup_restore | 20/23 - a wrong password starts a backup |
| M12 | ReadRange not enabled | event_log | 8/12 - ReadRange Rejected |
| M13 | Lift 1's alarm algorithm not armed | event_log | 10/12 - no notification is logged |
| M14 | Recipient_List not made writable | recipient_list | 4/10 - the writes are refused (PROPERTY / WRITE_ACCESS_DENIED), no alarm reaches the workstation |

M10 needed two test fixes before it failed: the first form (header check
removed) is still refused by the per-line check, so it removed nothing; and
the bad-image check originally restored the same value the stale image held,
and read `Last_Restore_Time` within the same second (it has one-second
resolution) - the test now changes Analog Value 100 to 9.0 first and waits a
second.
