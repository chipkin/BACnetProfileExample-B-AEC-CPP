# BACnet B-AEC (Advanced Elevator Controller) - C++ example

A tutorial example showing how to implement the BACnet **B-AEC (Advanced
Elevator Controller)** device profile, in C++, using the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack). It
answers **ReadProperty / ReadPropertyMultiple**, accepts **WriteProperty /
WritePropertyMultiple** to three commandable outputs and to the Elevator
Group's landing-call control, supports **SubscribeCOV** and
**SubscribeCOVPropertyMultiple**, generates **intrinsic alarms**
(EventNotifications when the Lift's `Passenger_Alarm` goes active), accepts
**AcknowledgeAlarm** and answers **GetEventInformation**, keeps an
**Event Log** (once a client enables it - see the status note below), drives
an output from a **Schedule**, handles
**DeviceCommunicationControl**, **initiates discovery** (Who-Is) on demand,
accepts **TimeSynchronization**, accepts **ReinitializeDevice** (including
Backup/Restore), and - the headline addition over B-EC - lets a BACnet client
**create and delete an Analog Value object at runtime** (CreateObject /
DeleteObject, DM-OCD-B). This example extends
[B-EC](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) with Event
Log, Schedule and Backup/Restore, and is the series' **canonical example for
F-OCD** (CreateObject/DeleteObject) - every later repository that needs
dynamic object creation copies this pattern.

**[Download a prebuilt binary](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP/releases)**
(Windows and Linux x64) - or build it yourself, see [Build](#build) below.

- **[TUTORIAL.md](TUTORIAL.md)** - how to extend this example and how to review
  it for conformance. Read it when you start turning this into your own device.
- **[docs/PICS.md](docs/PICS.md)** - the Protocol Implementation Conformance
  Statement: every object, every property, and who answers it.

> **Versions:** this document describes **example v1.3.0**, built and verified
> against **CAS BACnet Stack 6.0.23** (`6.x` @ `69b6249f`), at
> **Protocol_Revision 30**, with the vendored `common/` helper at **v3.5.0**.
> Running the example prints all three - if what it prints disagrees with this
> line, trust the program and check `CHANGELOG.md`.

## The scenario: an elevator a management station commands

What a B-AEC is for, quoted from ANSI/ASHRAE 135-2024 Annex L.13.1: **"A B-AEC
device performs control of elevators. It supports the modification and
commanding of its elevator objects by another device."** The same clause adds
object creation and deletion by another device, alarms logged in an Event Log,
schedules, clock synchronisation and configuration backup/restore. This example
does each of them against a Peer - an elevator management station, an
operator workstation, or the Python client in [`tests/`](tests/README.md):

1. **A landing call moves the car.** The Peer writes Elevator Group 1
   (`Maroon`)'s `Landing_Call_Control` - floor 6, up. Clause 12.58.10: "A write
   to this property is equivalent to a passenger pressing a call button at a
   landing". The call appears in `Maroon`'s `Landing_Calls` and in Lift 1
   (`Mauve`)'s `Assigned_Landing_Calls`; the car leaves floor 3 with
   `Car_Moving_Direction` `up`, `Car_Position` counts 4, 5, 6, the car stops,
   `Car_Door_Status` reads `opened`, the call leaves both lists, and 3 s later
   the door closes.
2. **Failure Mode - a floor that does not exist.** A landing call for floor 11
   (this installation serves floors 1-10) is refused with an Error and the car
   does not move.
3. **Objects created and deleted by another device (DM-OCD-B).** The Peer
   creates an Analog Value, writes it, deletes it. Creating a type this device
   does not create, a duplicate, or a ninth Analog Value is refused with the
   error clause 15.3.1.3.1 names; deleting Analog Output 1 is refused
   (OBJECT_DELETION_NOT_PERMITTED).
4. **The schedule runs on a clock the Peer sets.** TimeSynchronization to
   Monday 07:59:56; four seconds later, on the controller's own running clock,
   Schedule 1 (`Saffron`) moves Analog Output 1 (`Chartreuse`) from 20.0 to
   75.0 at priority 8.
5. **The Event Log captures the elevator's alarms.** Once the Peer writes Event
   Log 1 (`Beige`)'s `Enable` TRUE, each Passenger_Alarm transition of the Lift
   is logged, and ReadRange of `Log_Buffer` returns the notification.
6. **Backup and restore.** The Peer backs the configuration up (ReinitializeDevice
   START_BACKUP, AtomicReadFile of File 1, END_BACKUP), changes it, restores it
   (START_RESTORE, AtomicWriteFile, END_RESTORE), and the configuration is the
   backed-up one again. **Failure Modes:** a wrong password is refused
   (PASSWORD_FAILURE) and starts nothing; a restored file that is not a
   configuration image changes nothing.

Every one of these is a [Wire Test](#wire-tests). One stack limit at this pin
shows as a `[KNOWN GAP]` line and is in [TODO.md](TODO.md): the Event Log **starts
disabled** (a client must write `Enable` TRUE - the customer API has no call to
start it enabled, stack #3301). (Stack #3957, the device's own broadcast
notification logged twice, is fixed at this pin.) A second: the landing-call callback has no
error-code out-parameter, so the refusal in step 2 answers WRITE_ACCESS_DENIED,
not VALUE_OUT_OF_RANGE.

### What is Simulation

Non-BACnet scaffolding, outside the BACnet Surface, labelled `SIMULATION` in
[`main.cpp`](main.cpp):

- **The elevator car** behind Lift 1 (`Mauve`). A real B-AEC drives its
  elevator machinery over its own bus; this example has none, so a small model
  stands in: one car, floors 1-10, starting at floor 3, 2 s per floor, door open
  3 s at each stop, landing calls served first come first served with a stop at
  any floor on the way that also has a call. It is what `Car_Position`,
  `Car_Moving_Direction`, `Car_Door_Status`, `Landing_Door_Status`,
  `Landing_Calls` and `Assigned_Landing_Calls` report. Replace it with the
  interface to your elevator controller.
- **The Passenger_Alarm button** in the car, which flips every 30 s on a timer.
  The environment variable `BAEC_TEST_PASSENGER_ALARM_SECONDS=<n>` shortens
  that period for the Wire Tests; it is not a command-line option and changes
  nothing a BACnet client can reach.
- **The clock** is the host's clock plus the correction the last
  TimeSynchronization applied; a real device sets its RTC.

The BACnet behaviour around them - the landing-call write, the Lift's
properties, the alarm algorithm, the Event Log, the Schedule - is real.

## What this example supports

The example implements every BIBB the B-AEC profile requires, plus the
elevator object family inherited from B-EM/B-EC.

### BIBBs (BACnet Interoperability Building Blocks)

| BIBB | Description | Supported |
|------|-------------|:---------:|
| DS-RP-B | Data Sharing - ReadProperty - B | ✅ |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | ✅ |
| DS-WP-B | Data Sharing - WriteProperty - B | ✅ (F-OUTPUTS + Elevator Group's `Landing_Call_Control`) |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B | ✅ |
| DS-COV-B | Data Sharing - COV - B | ✅ |
| DS-COVM-B | Data Sharing - COV Multiple - B | ✅ |
| AE-N-I-B | Alarm and Event - Notification Internal - B | ✅ (intrinsic ChangeOfState on the Lift's `Passenger_Alarm`) |
| AE-ACK-B | Alarm and Event - ACK - B | ✅ |
| AE-INFO-B | Alarm and Event - Information - B | ✅ |
| AE-EL-I-B | Alarm and Event - Event Log - Internal - B | ✅ with caveats (NEW vs B-EC - canonical pattern from B-ALSC-CPP): starts disabled; see [TODO.md](TODO.md) |
| SCHED-I-B | Scheduling - Internal - B | ✅ (NEW vs B-EC - canonical pattern from B-AAC-CPP) |
| DM-DDB-A | Device Management - Dynamic Device Binding - A | ✅ (on demand - the `d` key) |
| DM-DDB-B | Device Management - Dynamic Device Binding - B | ✅ |
| DM-DOB-B | Device Management - Dynamic Object Binding - B | ✅ |
| DM-DCC-B | Device Management - Device Communication Control - B | ✅ |
| DM-TS-B | Device Management - TimeSynchronization - B | ✅ (local time only) |
| DM-OCD-B | Device Management - Object Creation and Deletion - B | ✅ **NEW vs B-EC - this repository is canonical for F-OCD** |
| DM-RD-B | Device Management - ReinitializeDevice - B | ✅ (COLDSTART and WARMSTART; the backup/restore states are DM-BR-B) |
| DM-BR-B | Device Management - Backup and Restore - B | ✅ (NEW vs B-EC - canonical pattern from B-ACC-CPP) |

### Services (executed / initiated)

| Service | Notes |
|---------|-------|
| ReadProperty, ReadPropertyMultiple | Responds to property reads (DS-RP-B, DS-RPM-B). |
| WriteProperty, WritePropertyMultiple | Commands the three outputs and the Elevator Group's landing call (DS-WP-B, DS-WPM-B). |
| SubscribeCOV, SubscribeCOVPropertyMultiple | Analog Input 1 is subscribable on two properties; its `COV_Increment` is 1.0. |
| ConfirmedEventNotification / UnconfirmedEventNotification | Sent to Notification Class 1's recipients when the Lift's `Passenger_Alarm` transitions. |
| AcknowledgeAlarm, GetEventInformation | Accepts alarm acknowledgement, answers event-information queries. |
| CreateObject, DeleteObject | Creates/deletes an Analog Value at runtime (DM-OCD-B). |
| AtomicReadFile, AtomicWriteFile | Backup/restore payload transfer against File 1. |
| ReadRange | Reads Event Log 1's `Log_Buffer` (AE-EL-I-B) - the only way to read it. |
| DeviceCommunicationControl | Accepts `disable-initiation` / `enable`, with an optional password. |
| ReinitializeDevice | Accepts COLDSTART/WARMSTART and the backup/restore states. |
| TimeSynchronization | Updates `Local_Date`/`Local_Time` from any TimeSynchronization request. |
| Who-Is / I-Am | Answers Who-Is with I-Am, broadcasts an I-Am at start-up, and **initiates** a demo Who-Is on the `d` key (DM-DDB-A). |
| Who-Has / I-Have | Answers Who-Has with I-Have. |

### Object types

| Object type | Instance | Name |
|-------------|:--------:|------|
| Device | 389014 | Chipkin Example B-AEC |
| Analog Input | 1 | Bronze |
| Binary Input | 1 | Emerald |
| Multi-State Input | 1 | Hot Pink |
| Analog Output | 1 | Chartreuse |
| Binary Output | 1 | Fuchsia |
| Multi-State Output | 1 | Indigo |
| Network Port | 1 | Vermilion |
| Elevator Group | 1 | Maroon |
| Lift | 1 | Mauve |
| Escalator | 1 | Mint |
| Positive Integer Value | 1 | Turquoise |
| Notification Class | 1 | Crimson |
| Event Log | 1 | Beige |
| Schedule | 1 | Saffron |
| Calendar | 1 | Cream |
| File | 1 | Ivory |
| Analog Value | *(dynamic)* | *created/deleted at runtime via DM-OCD-B* |

Every required property of every object, and who answers it, is in
[docs/PICS.md](docs/PICS.md).

## The device this example creates

```
Device 389014  "Chipkin Example B-AEC"   (Vendor 389 - Chipkin Automation Systems)
    ├── Analog Input  1            "Bronze"      read-only sensor (REAL, deg C); F-COVM demo (COV on 2 properties)
    ├── Binary Input  1            "Emerald"     read-only sensor (active/inactive)
    ├── Multi-State Input 1        "Hot Pink"    read-only sensor (state 1..3)
    ├── Analog Output 1            "Chartreuse"  REAL setpoint; commandable; Schedule 1 (Saffron) writes it at priority 8
    ├── Binary Output 1            "Fuchsia"     active/inactive; commandable
    ├── Multi-State Output 1       "Indigo"      state 1..3; commandable
    ├── Network Port 1             "Vermilion"   the BACnet/IP port (required)
    ├── Elevator Group 1           "Maroon"      groups Mauve; a write to Landing_Call_Control calls the car; Landing_Calls lists open calls
    ├── Lift 1                     "Mauve"       the car Simulation: Car_Position / Car_Moving_Direction / Car_Door_Status; simulated Passenger_Alarm -> intrinsic ChangeOfState ALARM
    ├── Escalator 1                "Mint"        not grouped (escalators aren't lifts)
    ├── Positive Integer Value 1   "Turquoise"   Maroon's Machine_Room_ID target (added before Maroon)
    ├── Notification Class 1       "Crimson"     routes Mauve's Passenger_Alarm events
    ├── Event Log 1                "Beige"       AE-EL-I-B; disabled until a client writes Enable TRUE; read with ReadRange
    ├── Schedule 1                 "Saffron"     SCHED-I-B; writes Chartreuse at priority 8
    ├── Calendar 1                 "Cream"       SCHED-I-B; Present_Value always FALSE, Date_List empty (see TODO.md)
    ├── File 1                     "Ivory"       DM-BR-B: holds the configuration image during a backup or restore
    └── Analog Value  (dynamic)    -             DM-OCD-B: created/deleted at runtime (up to 8); the configuration a backup saves
```

## Requires the CAS BACnet Stack (licensed product)

This example **builds against the CAS BACnet Stack, which is a commercial Chipkin
product** - it is not free or open source, and there is no public/trial build.
The stack is referenced here as the **private** git submodule
`submodules/cas-bacnet-stack`; you can only fetch and build it once you have a CAS
BACnet Stack license and access to that repository.

**To get the CAS BACnet Stack (and access to build this example), contact
Chipkin:** <https://store.chipkin.com/services/stacks/bacnet-stack> or
sales@chipkin.com.

You do not need a stack licence to *read* this example, or to run a
[prebuilt release binary](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP/releases).
The licence is what lets you *build* it - that is the part the stack submodule
gates.

## What's in this repository

This is a **self-contained** project. It ships:

- `main.cpp` - the example device.
- `common/` - the shared helper (UDP, callbacks, CLI, keyboard) vendored in.
- `CMakeLists.txt` - the build, the same on Windows, Linux, and macOS.
- `docs/PICS.md` - the conformance statement.
- `tests/` - the Wire Tests (see [Wire Tests](#wire-tests)), and `wiretest/`, the
  series' shared Python harness they are written against (vendored, like `common/`).
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack as a git submodule**
  (private; requires a license - see above). Its sources are compiled into the
  executable, so there is no library or DLL to build, ship, or install.

## Prerequisites

- A C++17 compiler (MSVC, GCC, or Clang).
- CMake >= 3.15.
- Git (to fetch the stack submodule).

### Windows

- **C++ compiler** - install
  [Visual Studio Community](https://visualstudio.microsoft.com/downloads/)
  (free) and select the **"Desktop development with C++"** workload.
- **CMake** - from <https://cmake.org/download/>, or `winget install Kitware.CMake`.

### Linux / macOS

- Debian/Ubuntu: `sudo apt install build-essential cmake git`
- macOS: `xcode-select --install` and `brew install cmake`

## Build

CMake only, and the same two commands on every platform:

```bash
git clone --recursive https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP.git
cd BACnetProfileExample-B-AEC-CPP

cmake -B build -S .
cmake --build build --config Release
```

Already cloned without `--recursive`? Run `git submodule update --init --recursive`
first - the build needs the stack submodule.

> **The first build takes a few minutes** - it compiles the entire CAS BACnet
> Stack (~600 source files) into the executable. Rebuilds after that are
> incremental and take seconds.

If your CAS BACnet Stack lives somewhere other than the bundled submodule, point
CMake at it: `cmake -B build -S . -D CAS_STACK_DIR=/path/to/cas-bacnet-stack`.

## Run

```bash
# Linux / macOS
./build/BACnetExampleBAEC

# Windows
.\build\Release\BACnetExampleBAEC.exe
```

Expected output:

```
BACnet B-AEC (Advanced Elevator Controller) Example - C++ v1.3.0
CAS BACnet Stack version: 6.0.23.0
Common helper (common/) version: 3.5.0
FYI: Listening for BACnet/IP on UDP port 47808 (Network Port 1).
TX 21 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1)
FYI: Device 389014 ("Chipkin Example B-AEC") ready. Vendor ID 389. Accepts WriteProperty to Chartreuse/Fuchsia/Indigo and Landing_Call_Control on Maroon. Press 'd' to broadcast a demo Who-Is (DM-DDB-A), or 'h' for help.
```

The `TX` line is the start-up I-Am the device broadcasts to announce itself. It
goes to the **local subnet broadcast** address (here `192.168.3.255`, computed
from the Network Port's interface), not the global `255.255.255.255`. As clients
talk to the device you'll see `RX ... bytes from ...` and `TX ... bytes to ...`
lines showing the traffic.

The device listens on UDP **47808** (BACnet/IP). Allow that port through your
firewall. To use a different port, pass `--port` (see below).

> **A few red `Error:` lines at start-up are expected and are not your bug.**
> They are shared with every example in the series (the device hearing its own
> broadcast I-Am, and a one-time BACnet/SC UUID notice).
> [TUTORIAL.md](TUTORIAL.md#troubleshooting) explains them. (The Event Log log
> flood earlier versions warned about, stack #2045, is fixed at this pin.)

### Command-line options

| Option | Default | Meaning |
|--------|---------|---------|
| `--port <n>` | `47808` | UDP port to listen on (BACnet/IP). |
| `--deviceID <n>` | `389014` | The device's BACnet instance number (BACnet requires this to be configurable). |
| `--deviceName <text>` | `Chipkin Example B-AEC` | The Device object's `Object_Name`, 1..255 bytes, no control characters. It must be unique across the BACnet internetwork, so give every unit its own (BACnet requires it to be configurable, like the instance). An invalid value prints a warning and keeps the default. |
| `--dcc-password <string>` | none | The password DeviceCommunicationControl and ReinitializeDevice (including backup and restore) must carry. Without it any request is accepted. |
| `--xml` | off | Print every received and sent BACnet message as an indented XML block instead of a one-line summary (very verbose; for protocol debugging). |
| `--help`, `-h` | - | Show usage and exit. |
| `--version` | - | Print the example, stack, and `common/` helper versions, then exit. |

### Interactive commands

While the example runs, these keys are available:

| Key | Action |
|-----|--------|
| `h` | Show the version information and this command list. |
| `q` | Quit. |
| up arrow | Increase Analog Input 1 (`Bronze`) by 1.1 (also the F-COVM demonstration). |
| down arrow | Decrease Analog Input 1 (`Bronze`) by 1.1. |
| `d` | Broadcast a demo Who-Is (DM-DDB-A). |
| `s` | Add a `Weekly_Schedule` transition for right now (SCHED-I-B demo). |

CreateObject/DeleteObject (DM-OCD-B) and WriteProperty are exercised over the
wire - there is no interactive key for them; use a BACnet client.

## Verify

**Automated, on the wire** - [`tests/`](tests/README.md) (Python 3 standard
library only). One command starts the controller once per test script and
drives it from a Python client - the Peer:

```bash
python3 tests/run_wire_tests.py --exe build/BACnetExampleBAEC              # Linux/macOS
python tests\run_wire_tests.py --exe build\Release\BACnetExampleBAEC.exe   # Windows
```

It ends with `ALL WIRE TESTS PASSED` (exit 0). Eight scripts, one per part of the
scenario above: discovery (every property every object lists reads as a value;
the Event Log's `Log_Buffer` is refused, as ReadRange only), the command line
(`--deviceID`, `--deviceName`, `--help`, `--xml`), the elevator,
CreateObject/DeleteObject, the Schedule over TimeSynchronization, the Event Log
over ReadRange, backup/restore, and the writable Notification Class
Recipient_List - with their Failure Modes. Each check has
been shown to fail with its feature removed (the table at the end of
[tests/README.md](tests/README.md)). A `[KNOWN GAP]` line is a stack defect,
listed in [TODO.md](TODO.md). CI runs them on Linux after the smoke test.

**By hand** - with a BACnet client such as the
[**CAS BACnet Explorer**](https://store.chipkin.com/products/tools/cas-bacnet-explorer):

1. **Discover** - Who-Is -> I-Am from `389014` (vendor `389`); `Object_List`
   lists the 17 base objects; `Protocol_Revision` = 30.
2. **Call the elevator** - WriteProperty Elevator Group 1 `Landing_Call_Control`
   with a BACnetLandingCallStatus (floor 7, direction up - raw `09 07 19 03`).
   Read Lift 1 `Car_Position` every second or two: 3, 4, 5, 6, 7, then
   `Car_Door_Status[1]` `opened`, then `closed`. The console prints every step.
3. **Create an object** - CreateObject `analog-value` 100, write its
   `Present_Value`, read it back, DeleteObject it.
4. **Run the schedule** - TimeSynchronization to a Monday at 07:59:50 and read
   Analog Output 1 `Present_Value` a few times: 20.0, then 75.0 after 08:00.
5. **Log an alarm** - WriteProperty Event Log 1 `Enable` TRUE, wait for the
   30-second Passenger_Alarm, ReadRange `Log_Buffer`.
6. **Back up and restore** - ReinitializeDevice STARTBACKUP, AtomicReadFile of
   File 1 (the text `B-AEC configuration 1` and one line per created Analog
   Value), ENDBACKUP; change a created Analog Value; STARTRESTORE,
   AtomicWriteFile of the saved bytes, ENDRESTORE; the value is back.
7. **F-OUTPUTS** - WriteProperty each output's `Present_Value` at a priority;
   relinquish (WriteProperty NULL) falls back to `Relinquish_Default`;
   out-of-range writes to Binary/Multi-State Output are rejected with
   `value-out-of-range`.
8. **Device management** - `DeviceCommunicationControl` `disable-initiation` /
   `enable` is accepted; `ReinitializeDevice` COLDSTART/WARMSTART returns a
   SimpleAck and resets commanded/simulated values (the car returns to floor
   3).

For a property-by-property review against the conformance statement, see
[TUTORIAL.md](TUTORIAL.md).

## Wire Tests

See [Verify](#verify) above and [tests/README.md](tests/README.md) for what
each check proves.

## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
There is one example repository per standardized device profile in ASHRAE 135-2024 Annex L. An example marked ⛔ is blocked: the stack cannot yet perform that profile's function through its customer interface, and the example's README says what it does today. Pick the profile your device claims, then the language you build in. "Ask" means the example hasn't been built yet for that language - [contact Chipkin](https://store.chipkin.com/contact-us) if you need one.

### Controllers (Annex L.4)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) | [B-SS-Node](https://github.com/chipkin/BACnetProfileExample-B-SS-Node) | [B-SS-CS](https://github.com/chipkin/BACnetProfileExample-B-SS-CS) | [B-SS-Rust](https://github.com/chipkin/BACnetProfileExample-B-SS-Rust) | [B-SS-Python](https://github.com/chipkin/BACnetProfileExample-B-SS-Python) | [B-SS-Go](https://github.com/chipkin/BACnetProfileExample-B-SS-Go) |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) | [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) | Ask | Ask | Ask | Ask |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Life safety controllers (Annex L.5)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 🚧 | Ask | Ask | Ask | Ask | Ask |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Access control controllers (Annex L.6)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) ⛔ blocked on [stack #2150](https://github.com/chipkin/cas-bacnet-stack/issues/2150), [#2044](https://github.com/chipkin/cas-bacnet-stack/issues/2044) | Ask | Ask | Ask | Ask | Ask |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) ⛔ blocked on [stack #2150](https://github.com/chipkin/cas-bacnet-stack/issues/2150), [#2044](https://github.com/chipkin/cas-bacnet-stack/issues/2044) | Ask | Ask | Ask | Ask | Ask |

### Lighting controllers (Annex L.11)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) | Ask | Ask | Ask | Ask | Ask |

### Elevator controllers (Annex L.13)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Authentication and authorization (Annex L.14)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) ⛔ blocked on [stack #2043](https://github.com/chipkin/cas-bacnet-stack/issues/2043) | Ask | Ask | Ask | Ask | Ask |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) ⛔ blocked on [stack #3934](https://github.com/chipkin/cas-bacnet-stack/issues/3934) | Ask | Ask | Ask | Ask | Ask |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) ⛔ blocked on [stack #2032](https://github.com/chipkin/cas-bacnet-stack/issues/2032) | Ask | Ask | Ask | Ask | Ask |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | — | — | — | — | — |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12)

Client-side profiles.

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-OWS** Operator Workstation | planned | — | — | — | — | — |
| **B-AWS** Advanced Operator Workstation | planned | — | — | — | — | — |
| **B-XAWS** Extended Advanced Operator Workstation | planned | — | — | — | — | — |
| **B-LSAP** Life Safety Annunciator Panel | planned | — | — | — | — | — |
| **B-LSWS** Life Safety Workstation | planned | — | — | — | — | — |
| **B-ALSWS** Advanced Life Safety Workstation | planned | — | — | — | — | — |
| **B-ACSD** Access Control Security Display | planned | — | — | — | — | — |
| **B-ACWS** Access Control Workstation | planned | — | — | — | — | — |
| **B-AACWS** Advanced Access Control Workstation | planned | — | — | — | — | — |
| **B-LOD** Lighting Operator Display | planned | — | — | — | — | — |
| **B-ALWS** Advanced Lighting Workstation | planned | — | — | — | — | — |
| **B-LCS** Lighting Control Station | planned | — | — | — | — | — |
| **B-ALCS** Advanced Lighting Control Station | planned | — | — | — | — | — |
| **B-ED** Elevator Display | planned | — | — | — | — | — |
| **B-EWS** Elevator Workstation | planned | — | — | — | — | — |
| **B-AEWS** Advanced Elevator Workstation | planned | — | — | — | — | — |

🚧 = in progress. ⛔ = Blocked Example (blocked on the named stack gap). "Ask" = not yet built for that language; contact Chipkin if you need it. Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
<!-- PROFILE-TABLE:END -->

## References

- **ANSI/ASHRAE Standard 135** (BACnet) - the protocol standard. Object model
  (Clause 12), alarming/events (Clause 13), services (Clause 16), device
  profiles (Annex L). Purchase / preview via the
  [ASHRAE store](https://www.ashrae.org/technical-resources/standards-and-guidelines).
- **What is BACnet?** - Chipkin's introduction:
  <https://docs.chipkin.com/protocols/bacnet/>.
- **CAS BACnet Stack** - product page and documentation:
  <https://store.chipkin.com/services/stacks/bacnet-stack>.
- **CAS BACnet Explorer** - client for testing this device:
  <https://store.chipkin.com/products/tools/cas-bacnet-explorer>.
- **B-EC example** (the seed this repository extends) -
  <https://github.com/chipkin/BACnetProfileExample-B-EC-CPP>.
- **B-ALSC example** (the AE-EL-I-B / Event Log pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP>.
- **B-AAC example** (the SCHED-I-B / Schedule pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP>.
- **B-ACC example** (the DM-BR-B / Backup-Restore pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP>.
- **Shared helper used by this example** - [`common/README.md`](common/README.md).

See also [TUTORIAL.md](TUTORIAL.md), [docs/PICS.md](docs/PICS.md),
[CHANGELOG.md](CHANGELOG.md), and [AGENTS.md](AGENTS.md).
