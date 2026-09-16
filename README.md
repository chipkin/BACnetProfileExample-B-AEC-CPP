# BACnet B-AEC (Advanced Elevator Controller) - C++ example

A tutorial example showing how to implement the BACnet **B-AEC (Advanced
Elevator Controller)** device profile, in C++, using the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack). It
answers **ReadProperty / ReadPropertyMultiple**, accepts **WriteProperty /
WritePropertyMultiple** to three commandable outputs and to the Elevator
Group's landing-call control, supports **SubscribeCOV** and
**SubscribeCOVPropertyMultiple**, generates **intrinsic alarms**
(EventNotifications when the Lift's `Passenger_Alarm` goes active), accepts
**AcknowledgeAlarm** and answers **GetEventInformation**, captures an
**Event Log**, drives an output from a **Schedule**, handles
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

> **Versions:** this document describes **example v1.0.0**, built and verified
> against **CAS BACnet Stack 6.0.21** (`6.x` @ `abd4cee1`), at
> **Protocol_Revision 24**, with the vendored `common/` helper at **v2.5.0**.
> Running the example prints all three - if what it prints disagrees with this
> line, trust the program and check `CHANGELOG.md`.

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
| AE-EL-I-B | Alarm and Event - Enrollment Log - Internal - B | ✅ (NEW vs B-EC - canonical pattern from B-ALSC-CPP) |
| SCHED-I-B | Scheduling - Internal - B | ✅ (NEW vs B-EC - canonical pattern from B-AAC-CPP) |
| DM-DDB-A | Device Management - Dynamic Device Binding - A | ✅ (on demand - the `d` key) |
| DM-DDB-B | Device Management - Dynamic Device Binding - B | ✅ |
| DM-DOB-B | Device Management - Dynamic Object Binding - B | ✅ |
| DM-DCC-B | Device Management - Device Communication Control - B | ✅ |
| DM-TS-B | Device Management - TimeSynchronization - B | ✅ (local time only) |
| DM-OCD-B | Device Management - Object Creation and Deletion - B | ✅ **NEW vs B-EC - this repository is canonical for F-OCD** |
| DM-RD-B | Device Management - ReinitializeDevice - B | ✅ (COLDSTART and WARMSTART) |
| DM-BR-B | Device Management - Backup and Restore - B | ✅ (NEW vs B-EC - canonical pattern from B-ACC-CPP) |

### Services (executed / initiated)

| Service | Notes |
|---------|-------|
| ReadProperty, ReadPropertyMultiple | Responds to property reads (DS-RP-B, DS-RPM-B). |
| WriteProperty, WritePropertyMultiple | Commands the three outputs and the Elevator Group's landing call (DS-WP-B, DS-WPM-B). |
| SubscribeCOV, SubscribeCOVPropertyMultiple | Analog Input 1 is subscribable on two properties. |
| ConfirmedEventNotification / UnconfirmedEventNotification | Sent to Notification Class 1's recipients when the Lift's `Passenger_Alarm` transitions. |
| AcknowledgeAlarm, GetEventInformation | Accepts alarm acknowledgement, answers event-information queries. |
| CreateObject, DeleteObject | Creates/deletes an Analog Value at runtime (DM-OCD-B). |
| AtomicReadFile, AtomicWriteFile | Backup/restore payload transfer against File 1. |
| DeviceCommunicationControl | Accepts `disable-initiation` / `enable`, with an optional password. |
| ReinitializeDevice | Accepts COLDSTART/WARMSTART and the backup/restore states. |
| TimeSynchronization | Updates `Local_Date`/`Local_Time` from any TimeSynchronization request. |
| Who-Is / I-Am | Answers Who-Is with I-Am, broadcasts an I-Am at start-up, and **initiates** a demo Who-Is on the `d` key (DM-DDB-A). |
| Who-Has / I-Have | Answers Who-Has with I-Have. |

### Object types

| Object type | Instance | Name |
|-------------|:--------:|------|
| Device | 389014 | Rainbow |
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
Device 389014  "Rainbow"   (Vendor 389 - Chipkin Automation Systems)
    ├── Analog Input  1            "Bronze"      read-only sensor (REAL, deg C); F-COVM demo (COV on 2 properties)
    ├── Binary Input  1            "Emerald"     read-only sensor (active/inactive)
    ├── Multi-State Input 1        "Hot Pink"    read-only sensor (state 1..3)
    ├── Analog Output 1            "Chartreuse"  REAL setpoint; commandable; Schedule 1 (Saffron) writes it at priority 8
    ├── Binary Output 1            "Fuchsia"     active/inactive; commandable
    ├── Multi-State Output 1       "Indigo"      state 1..3; commandable
    ├── Network Port 1             "Vermilion"   the BACnet/IP port (required)
    ├── Elevator Group 1           "Maroon"      groups Mauve; F-ELEVATOR; Landing_Call_Control writable
    ├── Lift 1                     "Mauve"       car position/doors/alarm; intrinsic ChangeOfState ALARM
    ├── Escalator 1                "Mint"        not grouped (escalators aren't lifts)
    ├── Positive Integer Value 1   "Turquoise"   Maroon's Machine_Room_ID target (added before Maroon)
    ├── Notification Class 1       "Crimson"     routes Mauve's Passenger_Alarm events
    ├── Event Log 1                "Beige"       AE-EL-I-B
    ├── Schedule 1                 "Saffron"     SCHED-I-B; writes Chartreuse at priority 8
    ├── Calendar 1                 "Cream"       SCHED-I-B; Date_List not populatable (issue #963)
    ├── File 1                     "Ivory"       DM-BR-B backup/restore payload
    └── Analog Value  (dynamic)    -             DM-OCD-B: created/deleted at runtime, not present at start-up
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
BACnet B-AEC (Advanced Elevator Controller) Example - C++ v1.0.0
CAS BACnet Stack version: 6.0.21.0
Common helper (common/) version: 2.5.0
FYI: Listening for BACnet/IP on UDP port 47808 (Network Port 1).
TX 21 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1)
FYI: Device 389014 ("Rainbow") ready. Vendor ID 389. Accepts WriteProperty to Chartreuse/Fuchsia/Indigo and Landing_Call_Control on Maroon. Press 'd' to broadcast a demo Who-Is (DM-DDB-A), or 'h' for help.
```

The `TX` line is the start-up I-Am the device broadcasts to announce itself. It
goes to the **local subnet broadcast** address (here `192.168.3.255`, computed
from the Network Port's interface), not the global `255.255.255.255`. As clients
talk to the device you'll see `RX ... bytes from ...` and `TX ... bytes to ...`
lines showing the traffic.

The device listens on UDP **47808** (BACnet/IP). Allow that port through your
firewall. To use a different port, pass `--port` (see below).

> **A wall of red `Error:` lines at start-up is expected and is not your bug -
> and on this device there are more of them than usual.** Two are shared with
> every example in the series (the device hearing its own broadcast I-Am, and a
> one-time BACnet/SC UUID notice); a third, unique to this example, is a
> **known, independently reproduced stack log flood** triggered by adding the
> Event Log object. [TUTORIAL.md](TUTORIAL.md#troubleshooting) explains all
> three.

### Command-line options

| Option | Default | Meaning |
|--------|---------|---------|
| `--port <n>` | `47808` | UDP port to listen on (BACnet/IP). |
| `--deviceID <n>` | `389014` | The device's BACnet instance number (BACnet requires this to be configurable). |
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

Use a BACnet client such as the
[**CAS BACnet Explorer**](https://store.chipkin.com/products/tools/cas-bacnet-explorer).
This repository's own verification session, against a
running instance of its own build:

1. **Discover** - Who-Is -> I-Am from `389014` (vendor `389`). ✅ verified.
2. **Object model** - `Object_List` lists all 17 base objects; `Protocol_Revision`
   = 24. ✅ verified.
3. **DM-OCD-B (this repository's canonical feature)** - `CreateObject(analog-value,
   2)` accepted; the object appears in `Object_List`; `ReadProperty` of its
   `Present_Value` (0.0) and `Object_Name` (`"undefined"`) both succeed;
   `WriteProperty` of `Present_Value` to 42.5 accepted and reads back
   correctly; `DeleteObject(analog-value, 2)` accepted; a subsequent
   `ReadProperty` of that instance correctly answers `Error(object:
   unknown-object)`; `CreateObject(binary-input, 5)` - not marked creatable -
   is correctly rejected with `CreateObjectError`. ✅ verified (see
   [TUTORIAL.md](TUTORIAL.md#who-serves-what-dm-ocd-bs-createobjectdeleteobject-cycle)
   for the full transcript).
4. **AE-EL-I-B** - `Object_Name`, `Record_Count`, `Total_Record_Count` on
   Beige all read correctly. ✅ verified. **Independently reproduced defect**:
   adding Beige causes a continuous internal log flood
   ([#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)) -
   functionally harmless, the device keeps answering correctly throughout -
   see [TUTORIAL.md](TUTORIAL.md#troubleshooting) and `TODO.md`.
5. **SCHED-I-B** - Saffron's `Priority_For_Writing` (8) and Cream's
   `Present_Value` (`false`) both read correctly. ✅ verified.
6. **DM-BR-B** - Ivory's `File_Size` reads back `0` correctly. This
   repository's own build did **not** reproduce B-ACC-CPP's documented
   read-access-denied finding for this same property - a genuine, new
   cross-repository discrepancy, not a re-assertion. ✅ verified; see `TODO.md`.
7. **F-OUTPUTS** - WriteProperty each output's `Present_Value` at a priority;
   relinquish (WriteProperty NULL) falls back to `Relinquish_Default`;
   out-of-range writes to Binary/Multi-State Output are rejected with
   `value-out-of-range`. ✅ verified.
8. **F-ELEVATOR** - every Elevator Group / Lift / Escalator required property
   reads correctly. `Landing_Call_Control`'s write path is registered and
   code-reviewed but was **not** independently wire-verified with a live
   client in this session - see `TODO.md`.
9. **Device management** - `DeviceCommunicationControl`
   `disable-initiation` / `enable` is accepted; `ReinitializeDevice`
   COLDSTART/WARMSTART returns a SimpleAck and resets commanded/simulated
   values as documented.

For a property-by-property review against the conformance statement, see
[TUTORIAL.md](TUTORIAL.md).


## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
The CAS BACnet Stack supports every standardized device profile in ASHRAE 135-2024 Annex L, and there is one example repository per profile. Pick the profile your device claims, then the language you build in. "Ask" means the example hasn't been built yet for that language - [contact Chipkin](https://store.chipkin.com/contact-us) if you need one.

### Controllers (Annex L.4)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) | Ask | Ask | Ask | Ask |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) | Ask | Ask | Ask | Ask |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) | [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) | Ask | Ask | Ask |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) | Ask | Ask | Ask | Ask |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) | Ask | Ask | Ask | Ask |

### Life safety controllers (Annex L.5)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 🚧 | Ask | Ask | Ask | Ask |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) | Ask | Ask | Ask | Ask |

### Access control controllers (Annex L.6)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) | Ask | Ask | Ask | Ask |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) | Ask | Ask | Ask | Ask |

### Lighting controllers (Annex L.11)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) | Ask | Ask | Ask | Ask |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) | Ask | Ask | Ask | Ask |

### Elevator controllers (Annex L.13)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) | Ask | Ask | Ask | Ask |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) | Ask | Ask | Ask | Ask |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) | Ask | Ask | Ask | Ask |

### Authentication and authorization (Annex L.14)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) | Ask | Ask | Ask | Ask |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) | Ask | Ask | Ask | Ask |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) | Ask | Ask | Ask | Ask |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) | Ask | Ask | Ask | Ask |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) | Ask | Ask | Ask | Ask |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) | Ask | Ask | Ask | Ask |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) | Ask | Ask | Ask | Ask |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) | Ask | Ask | Ask | Ask |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | — | — | — | — |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12)

Client-side profiles.

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) | Ask | Ask | Ask | Ask |
| **B-OWS** Operator Workstation | planned | — | — | — | — |
| **B-AWS** Advanced Operator Workstation | planned | — | — | — | — |
| **B-XAWS** Extended Advanced Operator Workstation | planned | — | — | — | — |
| **B-LSAP** Life Safety Annunciator Panel | planned | — | — | — | — |
| **B-LSWS** Life Safety Workstation | planned | — | — | — | — |
| **B-ALSWS** Advanced Life Safety Workstation | planned | — | — | — | — |
| **B-ACSD** Access Control Security Display | planned | — | — | — | — |
| **B-ACWS** Access Control Workstation | planned | — | — | — | — |
| **B-AACWS** Advanced Access Control Workstation | planned | — | — | — | — |
| **B-LOD** Lighting Operator Display | planned | — | — | — | — |
| **B-ALWS** Advanced Lighting Workstation | planned | — | — | — | — |
| **B-LCS** Lighting Control Station | planned | — | — | — | — |
| **B-ALCS** Advanced Lighting Control Station | planned | — | — | — | — |
| **B-ED** Elevator Display | planned | — | — | — | — |
| **B-EWS** Elevator Workstation | planned | — | — | — | — |
| **B-AEWS** Advanced Elevator Workstation | planned | — | — | — | — |

🚧 = in progress. "Ask" = not yet built for that language; contact Chipkin if you need it. Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
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
