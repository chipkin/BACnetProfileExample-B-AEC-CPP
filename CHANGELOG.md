# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

_No changes yet._

## [1.3.0] - 2026-10-27

Built on CAS BACnet Stack 6.x (69b6249f).

### Added

- **`--deviceName <text>`**: sets the Device's Object_Name at start-up, like
  `--deviceID` sets its instance. Object_Name must be unique on a BACnet
  internetwork, so two units no longer have to share the built-in name. The
  default is unchanged (`Chipkin Example B-AEC`). A name must be 1 to 255 bytes
  with no control characters, otherwise the example warns and keeps the default.
  The name cannot be changed over the network.
- **Automated self-tests** `tests/configuration_test.py` and
  `tests/recipient_list_test.py`.

### Changed

- **Notification Class 1's `Recipient_List` is writable** (cl. 12.21). The seeded
  entry stays the default; a management station can write the list, and the
  written list holds until the example restarts. A malformed write is refused.
- **File 1 (Ivory) `File_Size` is writable**, so a restore can empty the file
  first (Annex K.5.18). A smaller value truncates the file, 0 empties it, a larger
  one pads it with zero octets up to 4096; a larger size is refused with
  VALUE_OUT_OF_RANGE. Each write updates `Modification_Date`.
- **`--xml` now works.** It was listed in the help but ignored; each received and
  sent frame now prints as an XML block.
- **The Event Log logs each alarm transition once.** The device no longer logs its
  own broadcast notification a second time.
- **Self-tests** default to UDP port 47834 (was 48040).
- **Documentation**: PICS lists Database_Revision as 16 after start-up, changing
  by one on each accepted CreateObject and DeleteObject (cl. 12.11). It also
  lists the COV notifications the device initiates and the Lift's required
  properties (`Car_Position`, `Car_Moving_Direction`, `Car_Door_Status`,
  `Passenger_Alarm`, `Out_Of_Service`, `Fault_Signals`), the Elevator Group's
  writable `Landing_Call_Control` and the Event Log's `Stop_When_Full` and
  `Total_Record_Count`. The PICS objects tables cite the stack's property profiles.
- Vendored `common/` updated to 3.5.0 (see `common/CHANGELOG.md`).

### Fixed

- **Analog Input 1 now has `COV_Increment`** (reads 1.0), which cl. 12.2 requires
  on an Analog Input that supports COV reporting.
- **Event Log 1, Schedule 1, Calendar 1 and File 1 report their names** (Beige,
  Saffron, Cream, Ivory). They answered the stack's default names ("Object 25,1"
  and so on).
- **The Device's `Description` read as an empty string.** It was longer than the
  stack's 255-octet limit and was refused. It is now 243 characters, and the
  helper truncates to the limit.
- **Requests no longer go unanswered on Windows.** A request that arrived just as
  the socket's short receive timeout expired could be silently dropped. The
  receive loop now waits with `select()` (vendored `common/` 3.4.1).

## [1.2.0] - 2026-10-16

The example now performs the B-AEC Profile Function (ANSI/ASHRAE 135-2024 L.13.1):
a simulated elevator car moves in answer to landing calls, and backup and restore
work. Built on CAS BACnet Stack 6.x (64b2cfac5, reports 6.0.23).

### Added

- **The elevator moves.** A simulated car answers a landing call written to
  Elevator Group 1's `Landing_Call_Control`: the call is listed in
  `Landing_Calls` and Lift 1's `Assigned_Landing_Calls`, the car travels a floor
  every 2 s (`Car_Position`, `Car_Moving_Direction`), stops with the door open
  (`Car_Door_Status`, `Landing_Door_Status`), answers the call and closes the
  door. A call for a floor outside 1-10, a direction that is not up or down, a
  destination call to nowhere, or a ninth waiting call is refused.
- **A running device clock.** `Local_Date` and `Local_Time` are the host clock plus
  the offset set by the last TimeSynchronization, so Schedule 1 keeps running
  after a sync. The `s` key adds its transition on the device clock.
- **DM-BR-B backs up and restores the configuration.** A backup writes the created
  Analog Values and their values into File 1 as a text image; a restore makes the
  set exactly the backed-up one again and refuses a file that is not an image.
- **`--dcc-password`** is honoured: DeviceCommunicationControl and
  ReinitializeDevice, including the backup and restore states, require it when set.
- **`BAEC_TEST_PASSENGER_ALARM_SECONDS=<n>`** (environment variable, 1 to 3600)
  shortens the simulated Passenger_Alarm period for testing.
- **Automated self-tests** under `tests/` (Python standard library only) for
  discovery, the elevator, objects, the schedule, the event log and backup and
  restore.

### Changed

- Lift 1 `Registered_Car_Call` is an empty list, and `Assigned_Landing_Calls` and
  `Landing_Door_Status` follow the simulation instead of fixed values.
- **Protocol_Revision is 30.** Segmentation is supported in both directions
  (`Max_Segments_Accepted` 16).
- **Device renamed** from the placeholder "Rainbow" to "Chipkin Example B-AEC", so
  examples in this series can be told apart on one network.
- **Build**: the documented and released build compiles the stack from source with
  two commands (`cmake -B build -S .`, `cmake --build build --config Release`) on
  every platform. The prebuilt static library step is gone.
- **Documentation**: README is now this example only; configuration guidance and
  troubleshooting moved to a new `TUTORIAL.md`; a new `docs/PICS.md` lists the
  objects and properties. README, PICS and comments no longer claim behaviour the
  example did not have.
- Vendored `common/` updated to 3.4.0 (see `common/CHANGELOG.md`).
- `APP_VERSION` is 1.2.0.

### Fixed

- **DM-BR-B could never start.** ReinitializeDevice now accepts the backup and
  restore states (STARTBACKUP to ABORTRESTORE).
- **ReadRange is enabled** (AE-EL-I-B, Annex K.2.21), so the Event Log's
  `Log_Buffer` can be read.
- **File 1 (Ivory) `File_Type` and `Modification_Date`** are served instead of
  failing with read-access-denied. `File_Type` is `application/octet-stream`.
- **Elevator Group 1 `Landing_Calls` and `Landing_Call_Control`** read
  correctly instead of failing with read-access-denied.
- **`Application_Software_Version` and `Firmware_Revision`** now report the running
  build and linked stack version instead of a fixed "1.0.0".

### Compatibility

- Known limitations: the Event Log's `Enable` starts FALSE and must be written
  TRUE, and a landing-call write reports no specific error code on some refusals.
  See `TODO.md`.

## [1.0.0] - 2026-09-15

First release: a B-AEC (Advanced Elevator Controller) example, built from the B-EC
(Elevator Controller) example plus logging, scheduling, dynamic object creation
and backup and restore. Built on CAS BACnet Stack 6.x (abd4cee1, reports 6.0.21)
as a prebuilt static library.

### Added

- Everything B-EC has: ReadProperty, ReadPropertyMultiple, WriteProperty,
  WritePropertyMultiple, COV and COV-Multiple, alarm notification,
  AcknowledgeAlarm, GetEventInformation, discovery, DeviceCommunicationControl,
  time synchronization, ReinitializeDevice, three commandable outputs, the
  Elevator Group, Lift and Escalator objects, a Notification Class and an
  intrinsic alarm on the Lift's Passenger_Alarm.
- **DM-OCD-B**: a client can create and delete Analog Value objects at runtime
  (CreateObject and DeleteObject). Analog Value is the only creatable type, and a
  created object's `Present_Value` is writable.
- **Event Log 1 "Beige"** (AE-EL-I-B): the stack holds the log; `Enable` is writable.
- **Schedule 1 "Saffron" and Calendar 1 "Cream"** (SCHED-I-B): Saffron writes
  Analog Output 1 at priority 8 on a weekly schedule with a 2026-12-25 exception.
  The `s` key adds a transition for right now.
- **File 1 "Ivory"** (DM-BR-B): a stream-access file that carries the backup and
  restore payload; `Archive` is writable.
- `README.md`, `AGENTS.md`, `TODO.md` and a CMake build.

### Compatibility

- Known limitations in this release: the backup and restore half of DM-BR-B and a
  concurrent AtomicReadFile were not working, and adding the Event Log can produce
  a continuous internal "Failed to set the date" log message without affecting
  BACnet requests. Calendar 1's `Date_List` could not be populated, so Saffron's
  exception uses the inline calendar-entry form. See `TODO.md`.
