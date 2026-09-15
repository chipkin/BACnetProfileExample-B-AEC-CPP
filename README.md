# BACnet B-AEC (Advanced Elevator Controller) - C++ example

A tutorial example showing how to implement the BACnet **B-AEC (Advanced
Elevator Controller)** device profile, in C++, using the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack). It
answers **ReadProperty / ReadPropertyMultiple**, accepts **WriteProperty /
WritePropertyMultiple** to three commandable outputs and to the Elevator
Group's landing-call control, supports **SubscribeCOV** and
**SubscribeCOVPropertyMultiple**, generates **intrinsic alarms**
(EventNotifications when the Lift's Passenger_Alarm goes active), accepts
**AcknowledgeAlarm** and answers **GetEventInformation**, captures an
**Event Log**, drives an output from a **Schedule**, handles
**DeviceCommunicationControl**, **initiates discovery** (Who-Is) on demand,
accepts **TimeSynchronization**, accepts **ReinitializeDevice** (including
Backup/Restore), and - the headline addition over B-EC - lets a BACnet client
**create and delete an Analog Value object at runtime** (CreateObject/
DeleteObject, DM-OCD-B).

Part of the CAS BACnet Stack **BACnet profile example series** - one repository
per BACnet device profile. This example claims **only** B-AEC, and is the
series' **canonical example for F-OCD** (CreateObject/DeleteObject) - every
later repository that needs dynamic object creation copies this pattern.

> **Versions:** this document describes **example v1.0.0**, built and verified
> against **CAS BACnet Stack 6.0.21 (`6.x` @ `abd4cee1`)**, linked as a static
> library, at **Protocol_Revision 24**, with the vendored `common/` helper at
> **v2.5.0**. Running the example prints all three - if what it prints disagrees
> with this line, trust the program and check `CHANGELOG.md`.

This example seeds from [B-EC](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP)
(the Elevator *Controller*, which already has F-OUTPUTS/F-TIMESYNC/F-REINIT and
the elevator object family) and adds four features: **DM-OCD-B**
(CreateObject/DeleteObject - the profile's headline addition, canonical here),
**AE-EL-I-B** (Event Log, canonical pattern from
[B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP)),
**SCHED-I-B** (Schedule/Calendar, canonical pattern from
[B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP)), and
**DM-BR-B** (Backup/Restore, canonical pattern from
[B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP)).

## What is a B-AEC (Advanced Elevator Controller) profile?

A **device profile** (ANSI/ASHRAE 135, Annex L) lists the capabilities a class
of device must support. A **B-AEC** (Annex L.14) is B-EC (Annex L.13) plus
logging, scheduling, dynamic object management and backup/restore: like a B-EC
it reports what an elevator installation is doing and accepts commands, but it
also **logs events**, **schedules** an output, lets a management station
**create and delete an object**, and can be **backed up and restored**. (New to
BACnet? See Chipkin's [What is BACnet?](https://docs.chipkin.com/protocols/bacnet/)
guide.)

**What the profile requires** (and where this example stands):

| Requirement | BIBB | This example |
|---|---|:--:|
| ReadProperty | DS-RP-B | ✅ |
| ReadPropertyMultiple | DS-RPM-B | ✅ |
| WriteProperty | DS-WP-B | ✅ (F-OUTPUTS + Elevator Group's Landing_Call_Control) |
| WritePropertyMultiple | DS-WPM-B | ✅ |
| SubscribeCOV | DS-COV-B | ✅ |
| SubscribeCOVPropertyMultiple | DS-COVM-B | ✅ (unchanged from B-EC) |
| Generate event notifications | AE-N-I-B | ✅ (intrinsic ChangeOfState on the Lift's Passenger_Alarm) |
| Accept AcknowledgeAlarm | AE-ACK-B | ✅ |
| Answer GetEventInformation | AE-INFO-B | ✅ |
| Event Log | AE-EL-I-B | ✅ (NEW vs B-EC - canonical pattern from B-ALSC-CPP) |
| Schedule | SCHED-I-B | ✅ (NEW vs B-EC - canonical pattern from B-AAC-CPP) |
| Initiate discovery (Who-Is) | DM-DDB-A | ✅ (on demand - the `d` key) |
| Who-Is/I-Am (answer), Who-Has/I-Have | DM-DDB-B, DM-DOB-B | ✅ |
| DeviceCommunicationControl | DM-DCC-B | ✅ |
| TimeSynchronization | DM-TS-B (or DM-UTC-B) | ✅ (local time only, same choice B-LD/B-EC make) |
| **CreateObject/DeleteObject** | **DM-OCD-B** | ✅ **NEW vs B-EC - this repository is canonical for F-OCD** |
| ReinitializeDevice | DM-RD-B | ✅ (COLDSTART and WARMSTART) |
| Backup and Restore | DM-BR-B | ✅ (NEW vs B-EC - canonical pattern from B-ACC-CPP; backup verified, restore/concurrent-AtomicReadFile carried as a documented gap - see `TODO.md`) |

## F-OUTPUTS: three commandable outputs (the headline delta versus B-EM)

**Analog Output 1 "Chartreuse"** (REAL setpoint), **Binary Output 1 "Fuchsia"**
(active/inactive) and **Multi-State Output 1 "Indigo"** (state 1..3) are each
commandable: a 16-slot `Priority_Array` plus a `Relinquish_Default`, following
the canonical pattern from [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP)
byte-for-byte (the `Commandable` struct, `CommandWrite`/`CommandRelinquish`,
`SetPropertyWritable(Present_Value)`). WriteProperty at a priority commands a
slot; WriteProperty of NULL relinquishes it; `Present_Value` always reflects
the highest-priority non-null slot, or `Relinquish_Default` when every slot is
null. Out-of-range writes (Binary Output outside 0/1, Multi-State Output
outside 1..`Number_Of_States`) are rejected with `value-out-of-range` -
verified by wire test.

## F-ELEVATOR: the elevator object family, now partly writable

**Elevator Group 1 "Maroon"**, **Lift 1 "Mauve"** and **Escalator 1 "Mint"**
are unchanged from B-EM - see that repository's README for the full object
family writeup. The one genuinely new piece: B-EM deliberately left the fifth
elevator callback, `BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl`,
**unregistered**, which kept `Landing_Call_Control` non-writable. This example
**registers it**, so a WriteProperty naming a floor/direction/door is recorded
and read back through the same `GetListElevatorGroupLandingCallStatus`
callback B-EM already registered (now also serving `Landing_Call_Control`,
distinguished from `Landing_Calls` by the `propertyIdentifier` parameter).

## Intrinsic alarming: the Lift's Passenger_Alarm

Unchanged from B-EM. **Lift 1 "Mauve"** arms an intrinsic **ChangeOfState**
(boolean) algorithm on its `Passenger_Alarm` property: `Passenger_Alarm ==
true` is the OFFNORMAL condition. On each transition the stack sends an
**EventNotification** to the recipients of **Notification Class 1 "Crimson"**.
`Passenger_Alarm` is not writable through any API in this stack (a real
installation drives it from hardware), so this example **simulates** it on a
30-second timer, exactly as B-EM does.

## DM-TS-B: TimeSynchronization

`Local_Date`/`Local_Time` (Device object) are seeded from the host clock at
start-up and updated by any TimeSynchronization request thereafter - the
canonical pattern from [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP),
copied here verbatim. Only `TimeSynchronization` is enabled (not
`UTCTimeSynchronization`) - this profile allows either, and this example does
local time only, the same choice B-LD makes.

## DM-RD-B: ReinitializeDevice

A management station's ReinitializeDevice (COLDSTART or WARMSTART) is
accepted, ACKed, and acted on from the main loop after a short deferred delay
(`CASExampleHelper::RequestRestart`/`RestartDue`) so the SimpleACK reaches the
wire before anything resets - the canonical pattern shared by
B-AAC/B-ASC/B-LSC. COLDSTART restores every commanded/simulated value to its
power-on state (the three outputs relinquish to their `Relinquish_Default`,
the elevator alarm/out-of-service flags clear, the queued landing call is
forgotten) and re-announces with an I-Am; WARMSTART re-initializes
communications but keeps the commanded outputs. Verified by wire test: a
COLDSTART request returns a SimpleAck and Analog Output 1 reads back its
`Relinquish_Default` (20.0) afterward.

## DM-DDB-A: discovery on demand

Unlike B-EM (which only answers Who-Is), this profile also **initiates**
discovery. Press **`d`** to broadcast a demo Who-Is - the same key
[B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) claims for
its own remote-discovery demo (`docs/menu-keys.md`); no new key was needed.

## DM-OCD-B: CreateObject/DeleteObject (the headline addition, canonical here)

A BACnet client can **create** an Analog Value object at runtime with the
CreateObject service, and **delete** it again with DeleteObject.
`BACnetStack_SetObjectTypeCreatable(analog-value, true)` is the only object
type this example marks creatable - every other object type in this file is
fixed, added once at start-up the same way every other example in the series
does. `BACnetStack_RegisterCallbackCreateObject`/`RegisterCallbackDeleteObject`
are the two entry points: CreateObject fires **before** the object exists in
the stack's own database (its doc comment is explicit about this), so this
file owns a small host-side table (`g_createdAnalogValues`) that IS the
object's storage - the `GetPropertyReal`/`SetPropertyReal` branches for
`OBJECT_TYPE_ANALOG_VALUE` consult only that table, never
`BACnetStack_AddObject`'s per-object state (which is never called for this
type). A created Analog Value's `Present_Value` is made writable at the
**object-TYPE** level (`BACnetStack_SetPropertyByObjectTypeWritable`, applied
to the stack's default template for objects created later) since there is no
per-instance object to configure before a client creates one.

Verified over the wire with a live `bacpypes3` client against this
repository's own build: `CreateObject(analog-value, 2)` is accepted
(CreateObjectACK) and the object immediately appears in `Object_List`;
`ReadProperty` of its `Present_Value` (0.0) and `Object_Name` ("undefined",
the stack's generic non-Device default) both succeed; `WriteProperty` of
`Present_Value` to 42.5 is accepted and reads back correctly;
`DeleteObject(analog-value, 2)` is accepted (SimpleACK) and a subsequent
`ReadProperty` of that instance correctly answers `Error(object:
unknown-object)`; `CreateObject(binary-input, 5)` - a type that was never
marked creatable - is correctly rejected with a `CreateObjectError`, never
reaching this file's `CreateObject` callback in a way that allocates
anything.

## AE-EL-I-B: Event Log (NEW vs B-EC)

Event Log 1 "Beige" (`BACnetStack_AddEventLogObject`) - canonical pattern
copied byte-for-byte from
[B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP).
Every property Beige exposes (`Enable`, `Buffer_Size`, `Record_Count`,
`Total_Record_Count`, `Log_Buffer`, ...) is held and served by the stack's own
Event Log engine, not by a `Get*`/`Set*` callback in this file; `Enable`
(`Log_Enable`) is writable. Verified over the wire: `Object_Name`,
`Record_Count` and `Total_Record_Count` all read correctly.

**Known stack defect, independently reproduced here
([chipkin/cas-bacnet-stack#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045),
first found by B-ACC-CPP):** adding this object causes a continuous, non-fatal
`Failed to set the date` / `Failed to set the time` internal log flood,
starting from the very first `Tick()` - 200 occurrences of each line were
observed in a 5-second smoke test window in this session. **This is a new,
independent data point, not a re-assertion:** B-ACC-CPP's own bisection
concluded the trigger was `AddEventLogObject` in isolation, but
[B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) adds
the identical object with the identical call and its own task explicitly did
**not** observe the flood. This repository's object set overlaps with both:
Event Log + Schedule/Calendar (like B-ALSC, no flood there) **plus** File/
Backup-Restore (like B-ACC, flood observed there too) - so the trigger looks
more like "Event Log combined with File/Backup-Restore" than "Event Log
alone", though this was not re-isolated by bisection in this session. See
`TODO.md` for the full comparison. The device keeps answering Who-Is and
ReadProperty correctly throughout the flood (also verified over the wire).

## SCHED-I-B: Schedule (NEW vs B-EC)

Schedule 1 "Saffron" plus Calendar 1 "Cream" - canonical pattern from
[B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP), copied
via B-ALSC-CPP's more recent template. Saffron writes Analog Output 1
"Chartreuse" `Present_Value` at priority 8 whenever a `Weekly_Schedule` or
`Exception_Schedule` entry is active (one weekly transition, Monday 08:00, and
one calendar-date exception for 2026-12-25); `Schedule_Default` applies the
rest of the time. Every Schedule-owned property is genuinely held by the
stack's own Schedule engine (`BACnetStack_AddScheduleObject` plus the
`BACnetStack_SetSchedule*`/`AddSchedule*` configuration calls), not a
`Get*`/`Set*` callback. Cream exists for SCHED-I-B completeness but its
`Date_List` cannot be populated through the customer API (stack issue #963,
the same inherited gap B-AAC/B-ALSC/B-ACC all document) - Saffron's exception
uses the inline calendar-entry form instead, so this does not affect its own
behaviour. Verified over the wire: `Priority_For_Writing` (8) and Cream's
`Present_Value` (false) both read correctly.

## DM-BR-B: Backup and Restore (NEW vs B-EC)

File 1 "Ivory" (`BACnetStack_AddFileObject`, stream access, in-memory 4KB
buffer) is the backup/restore payload carrier; `BACnetStack_SetBackupAndRestoreEnabled`
plus the four Prepare/Complete Backup/Restore callbacks are registered.
`ReinitializeDevice` accepts the backup/restore states - canonical pattern
from [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP).
Verified over the wire: `File_Size` reads back 0 (empty file) correctly - this
repository's own build did **not** reproduce B-ACC-CPP's documented
`File_Size` read-access-denied finding (see `TODO.md` for the comparison).
`AtomicReadFile`/`AtomicWriteFile` round-tripping and the restore half of the
backup/restore cycle were **not** exercised over the wire in this session -
carried forward from B-ACC-CPP's own documented, unresolved gap there rather
than re-tested; see `TODO.md`.

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
    ├── Event Log 1                "Beige"       AE-EL-I-B (NEW vs B-EC)
    ├── Schedule 1                 "Saffron"     SCHED-I-B (NEW vs B-EC); writes Chartreuse at priority 8
    ├── Calendar 1                 "Cream"       SCHED-I-B (NEW vs B-EC); Date_List not populatable (issue #963)
    ├── File 1                     "Ivory"       DM-BR-B backup/restore payload (NEW vs B-EC)
    └── Analog Value  (dynamic)    -             DM-OCD-B (NEW vs B-EC): created/deleted at runtime, not present at start-up
```

The three inputs plus the Network Port are the series' shared minimum;
Chartreuse/Fuchsia/Indigo through Crimson are unchanged from B-EC; Beige,
Saffron, Cream, Ivory and the dynamically-creatable Analog Value are this
repository's additions. Object names follow the series' colour convention
(Device is always "Rainbow").

## What this example does NOT do yet

Every BIBB B-AEC requires is implemented against the pinned stack - including
the profile's headline requirement, DM-OCD-B, which is fully implemented and
wire-verified (see above). What is **not** fully demonstrated:

- **AE-EL-I-B's alarm-capture demo**: Beige's `AddEventLogObject` causes a
  documented, independently-reproduced stack log flood
  ([#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)) -
  functionally harmless in this session's testing but carried forward
  undisguised rather than avoided. See `TODO.md` #1.
- **DM-BR-B's restore half and concurrent AtomicReadFile**: backup
  (`ReinitializeDevice(startBackup)`/`endBackup`) was not independently
  wire-verified in this session (carried forward from B-ACC-CPP's own
  documented gap, not re-tested); restore and a concurrent AtomicReadFile were
  found broken by B-ACC-CPP
  ([#2046](https://github.com/chipkin/cas-bacnet-stack/issues/2046)) and are
  carried forward, not re-tested here. See `TODO.md` #2.
- **Calendar 1 (Cream)'s `Date_List`**: cannot be populated through the
  customer API (stack issue #963, inherited from B-AAC/B-ALSC/B-ACC). See
  `TODO.md` #3.

**One item flagged, not faked (inherited from B-EC):** this example registers
`RegisterCallbackSetElevatorGroupLandingCallControl` and the code was reviewed
against the stack's doc comment for correctness, but a WriteProperty of
`Landing_Call_Control` was **not** independently wire-verified with a live
BACnet client in this repository's verification session -
`BACnetLandingCallStatus`'s write-side SEQUENCE-CHOICE encoding could not be
cleanly expressed through the test-client library available in that session.
See `docs/objects.json`'s note on Maroon for detail.

## Before you ship

This example is a tutorial, and it identifies itself as one. Everything in this
table is read by clients and shown to the operator in **every discovery tool on
the network**. Left as-is, your product appears on a real site announcing itself
as a Chipkin demo. None of it is cosmetic.

| Constant (`main.cpp`) | Ships as | Change it to |
|---|---|---|
| `VENDOR_IDENTIFIER` | `389` (Chipkin) | **Your** company's vendor ID. Assigned by ASHRAE, free: <https://bacnet.org/assigned-vendor-ids/> |
| `VENDOR_NAME` | `Chipkin Automation Systems` | Your company name - must match the vendor ID above. |
| `DEVICE_NAME` | `"Rainbow"` | Your device's `Object_Name`. **Must be unique across the BACnet internetwork.** |
| `MODEL_NAME` | `CAS BACnet Stack Example - B-AEC` | Your model designation - what a building operator reads to identify your device. |
| `DEVICE_DESCRIPTION` | a description of *this example* | What your device actually is. |
| `FIRMWARE_REVISION` / `APPLICATION_SOFTWARE_VERSION` | `1.0.0` | Your real versions - wire them to your build. |
| `DCC_PASSWORD` | `""` (no password) | Set your device's secret, or leave empty to accept any DeviceCommunicationControl/ReinitializeDevice. It crosses the wire in **plaintext** - a guard against accidents, not a security boundary. |
| Device instance | `389014` (`--deviceID` overrides) | Must be unique on the internetwork. BACnet requires this to be configurable; keep it so. |

`main.cpp` marks this block with a `CHANGE ALL OF THIS BEFORE YOU SHIP` banner.

## Extending the example

If you copy this file as the seed for a profile that adds more elevator
commands or objects, walk every `Get*`/`Set*` callback listed in
`main.cpp`'s "ADDING AN OBJECT? READ THIS FIRST" comment, then **read back
every required property and diff it against a working object** - a
half-added object is not caught by "it scanned OK"; the stack silently
substitutes defaults (`Object_Name` -> `"undefined"`, `Units` -> `no-units`,
otherwise a datatype zero) for most properties a `Get` callback declines. A
real bug this exact mistake caused, caught during this repository's own
implementation: omitting `Relinquish_Default` from Binary Output 1's Get
callback (present for Analog and Multi-State Output, forgotten for Binary
Output) made **both** `Relinquish_Default` and `Present_Value` fail with
`read-access-denied` on the wire, because the stack could not resolve
Present_Value without it - caught only by reading back every property of every
new object with a real client, not by "it built with zero warnings."

## Requires the CAS BACnet Stack (licensed product)

This example **builds against the CAS BACnet Stack, a commercial Chipkin product** -
not free or open source, no public/trial build. The stack is the **private** git
submodule `submodules/cas-bacnet-stack`; you can only fetch and build it with a CAS
BACnet Stack license. **To get the stack, contact Chipkin:**
<https://store.chipkin.com/services/stacks/bacnet-stack> or sales@chipkin.com. You
do not need a stack licence to *read* this example's own source: every file outside
submodules/ is CC0 public domain. The licence is what lets you *build* it.

## Build

This example links the CAS BACnet Stack as a prebuilt **STATIC** library. Build
the library once from the pinned submodule commit, then configure and build the
example against it:

```bash
git clone --recursive https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP.git
cd BACnetProfileExample-B-AEC-CPP
git submodule update --init --recursive   # if not cloned with --recursive
tools/build-stack-static.sh BACnetProfileExample-B-AEC-CPP   # from the series root; builds
                                                              # submodules/cas-bacnet-stack/bin/...
cmake -B build -S . -DCAS_BACNET_STACK_LINK=STATIC
cmake --build build --config Release
./build/BACnetExampleBAEC            # Linux/macOS
.\build\Release\BACnetExampleBEC.exe   # Windows
```

> **The stack library build takes a few minutes** the first time - it compiles
> the entire CAS BACnet Stack (~600 source files) once, via the stack's own
> project files (`msbuild` on Windows, `make` on Linux). The example itself
> (`main.cpp` + `common/`) then builds in seconds against that library, and
> rebuilds after that are incremental.

Use `-D CAS_STACK_DIR=/path` to point at a stack elsewhere. Options: `--port <n>`
(default 47808), `--deviceID <n>` (default 389014), `--help` (show usage and exit),
`--version` (print the example, stack, and `common/` versions and exit).
Interactive keys: `h` help, `q` quit, up/down nudge Analog Input 1 (also the
F-COVM demonstration), `d` broadcast a demo Who-Is (DM-DDB-A), `s` add a
Weekly_Schedule transition for right now (SCHED-I-B demo, `common/` 2.1.0+).

### Link mode

This example links the stack through the `CASBACnetStack::Adapter` CMake target
(`submodules/cas-bacnet-stack/adapters/cpp`) in **STATIC** mode -
`-DCAS_BACNET_STACK_LINK=STATIC` links the prebuilt
`CASBACnetStack_x64_Release.lib` / `libCASBACnetStack_x64_Release.a` built by
`tools/build-stack-static.sh` above. **Application code is identical
regardless of link mode** - `main.cpp` and `common/` call `BACnetStack_AddDevice(...)`
and friends by the exact export name. Every mode requires calling
`LoadBACnetFunctions()` once at the top of `main()` before any other
`BACnetStack_*` call, which runs a version handshake; if it fails,
`CASBACnetStackAdapter_LastError()` says why and the program exits with a
message rather than crashing.

The adapter also offers a **SOURCE** mode (compiles the stack's `source/*.cpp`
straight into the executable, no library build step) - this example is built
and published in **STATIC** mode only.

## Verify

With the [CAS BACnet Explorer](https://store.chipkin.com/products/tools/cas-bacnet-explorer)
(or any client, e.g. `bacpypes3`/`BAC0`). **This session's own verification
against a running instance of THIS repository's build** (via `bacpypes3`)
focused on the four features new versus B-EC, since F-OUTPUTS/F-ELEVATOR/
F-COVM/alarming/DM-TS-B/DM-RD-B/DM-DDB-A are unchanged code inherited from
B-EC and already wire-verified there (see B-EC-CPP's own README):

1. **Discover** - Who-Is -> I-Am from `389014` (vendor `389`). ✅ verified this session.
2. **Object model** - `Object_List` lists all 17 base objects (the ten
   inherited from B-EC plus Beige, Saffron, Cream, Ivory); `Protocol_Revision`
   = 24. ✅ verified this session.
3. **DM-OCD-B (this repository's canonical feature)** - `CreateObject(analog-value,
   2)` accepted; the object appears in `Object_List`; `ReadProperty` of its
   `Present_Value` (0.0) and `Object_Name` ("undefined") both succeed;
   `WriteProperty` of `Present_Value` to 42.5 accepted and reads back
   correctly; `DeleteObject(analog-value, 2)` accepted; a subsequent
   `ReadProperty` of that instance correctly answers `Error(object:
   unknown-object)`; `CreateObject(binary-input, 5)` - not marked creatable -
   is correctly rejected with `CreateObjectError`. ✅ all verified this
   session (see `docs/objects.json`'s Analog Value entry for the full
   transcript).
4. **AE-EL-I-B** - `Object_Name`, `Record_Count`, `Total_Record_Count` on
   Beige all read correctly. ✅ verified this session. **Independently
   reproduced defect**: adding Beige causes a continuous internal log flood
   (issue #2045) - see "What this example does NOT do yet."
5. **SCHED-I-B** - Saffron's `Priority_For_Writing` (8) and Cream's
   `Present_Value` (false) both read correctly. ✅ verified this session. The
   scheduled write itself (Chartreuse moving at the scheduled time, or via the
   `s` DemoAdvance key) was not exercised this session.
6. **DM-BR-B** - Ivory's `File_Size` reads back 0 correctly (this
   repository's own build did **not** reproduce B-ACC-CPP's documented
   read-access-denied finding for this property). ✅ verified this session.
   Backup/restore state transitions and AtomicReadFile/AtomicWriteFile were
   **not** exercised this session - see "What this example does NOT do yet."
7. **F-OUTPUTS, F-ELEVATOR, F-COVM, alarming, DM-TS-B, DM-RD-B, DM-DDB-A** -
   unchanged from B-EC-CPP, which wire-verified all of these against its own
   build (WriteProperty at priorities with relinquish, out-of-range rejection,
   SubscribeCOVPropertyMultiple, TimeSynchronization, ReinitializeDevice
   COLDSTART, and a demo Who-Is). Not independently re-run against this
   repository's build this session, since the code paths are byte-for-byte
   identical to B-EC's already-verified ones. B-EC's one flagged, unverified
   item (`Landing_Call_Control`'s write path, code-reviewed but not
   wire-tested) carries forward unchanged - see "What this example does NOT
   do yet."
8. **Device management** - DeviceCommunicationControl `disable-initiation` /
   `enable` is accepted (unchanged from B-EC).

## What's in this repository

`main.cpp` (the example), `common/` (the vendored shared helper), and
`submodules/cas-bacnet-stack/` (the CAS BACnet Stack as a private git submodule,
compiled from source). Self-contained: clone with `--recursive` and build.

## Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the stack's `docs/property-profile-reference.md`), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Analog Input 1 "Bronze" - REAL, degrees Celsius; starts at 21.5. F-COVM: Present_Value AND Status_Flags are both marked subscribable (SetPropertySubscribable), so a SubscribeCOVPropertyMultiple naming both fires one multi-property notification when the up/down keys nudge this value

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |

### Binary Input 1 "Emerald" - starts inactive

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |

### Multi-state Input 1 "Hot Pink" - state 1 of 3: On, Off, Auto

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| State_Text *(optional, enabled)* | BACnetARRAY[N] of CharacterString | app | no |

### Analog Output 1 "Chartreuse" - F-OUTPUTS (NEW vs B-EM; canonical pattern from B-SA-CPP). REAL setpoint, 16-slot Priority_Array + Relinquish_Default (20.0); commandable. Present_Value itself is resolved by the stack from the Priority_Array - this application's Get callback answers the individual array-index reads (and Relinquish_Default) rather than Present_Value directly, exactly as B-SA's canonical pattern documents

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalReal | app | no |
| Relinquish_Default | Real | app | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Binary Output 1 "Fuchsia" - F-OUTPUTS (NEW vs B-EM). Active/inactive, commandable; Relinquish_Default inactive (0). Out-of-range writes (anything but 0/1) are rejected with value-out-of-range

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | app | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalBinaryPV | app | no |
| Relinquish_Default | BACnetBinaryPV | app | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Multi-state Output 1 "Indigo" - F-OUTPUTS (NEW vs B-EM). State 1..3, commandable; Relinquish_Default state 1. Out-of-range writes (state 0 or >3) are rejected with value-out-of-range. Verified by wire test: WriteProperty of state 9 rejected with value-out-of-range

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalUnsigned | app | no |
| Relinquish_Default | Unsigned | app | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Network Port 1 "Vermilion" - BACnet/IP; Network_Type and Protocol_Level are set from BACnetStack_AddNetworkPortObject()'s arguments (IPv4, BACnet Application) at start-up, not a GetProperty callback like the object's other app-served rows; Changes_Pending is likewise computed and answered natively by the stack's Network Port object. Reliability has no fault condition this example detects, so it is accepted at the generic default (normal)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | app | no |
| Protocol_Level | BACnetProtocolLevel | app | no |
| Changes_Pending | Boolean | app | no |

### Elevator Group 1 "Maroon" - F-ELEVATOR (canonical, from B-EM). Machine_Room_ID, Group_ID and Group_Members are NOT stack DEFAULTS - they are genuinely stored and served by BACnetStack_AddElevatorGroupObject (Group_Members is also maintained by BACnetStack_AddLiftOrEscalatorObject as Lift 1/Mauve is added to this group). They are marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API and so cannot credit them as stack-served. isGroupOfLifts=true and supportLandingCallStatus=true: Group_Mode and LandingCalls are therefore also enabled (see the Lift's note for how the four F-ELEVATOR list/sequence callbacks are exercised) but property-profile-reference.md's generic Elevator Group table does not mark either 'required', so neither generates a checked row here - Group_Mode is served by GetPropertyEnumerated (normal) and LandingCalls by GetListElevatorGroupLandingCallStatus (no calls outstanding) regardless. NEW vs B-EM: Landing_Call_Control is now writable - BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl is registered (unlike B-EM, which deliberately left it unregistered), so a WriteProperty commanding a landing call is recorded and read back through GetListElevatorGroupLandingCallStatus. Code-reviewed against the stack's doc comment and confirmed to build/register correctly; NOT independently wire-verified with a real client in this session (BACnetLandingCallStatus's write-side SEQUENCE-CHOICE encoding is not cleanly expressible through the available test-client library) - flagged in the PR rather than assumed

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Machine_Room_ID | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Group_Members | BACnetARRAY[N] of BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |

### Lift 1 "Mauve" - F-ELEVATOR (canonical). property-profile-reference.md's generated Lift table is INCOMPLETE relative to CASBACnetStackDLL.h's own doc comment for BACnetStack_AddLiftOrEscalatorObject (a documentation gap in the stack repo, not a stack API gap): the markdown table omits Car_Position, Car_Moving_Direction, Car_Door_Status, Passenger_Alarm, Out_Of_Service and Fault_Signals entirely, even though the DLL header lists all six as REQUIRED Lift properties. This example serves every one of them regardless, because they are genuinely required by clause 12.59 - Car_Position and Car_Moving_Direction via GetPropertyUnsignedInteger/GetPropertyEnumerated, Car_Door_Status (one door, index 1) via GetPropertyEnumerated with useArrayIndex, Passenger_Alarm and Out_Of_Service via GetPropertyBool, Fault_Signals (empty - no active faults) via the GetListOfEnumerations callback. Status_Flags, Elevator_Group, Group_ID and Installation_ID ARE in the generic table (stack-computed/stack-stored) and are accepted for the same reason as Maroon's. Passenger_Alarm is the AE-N-I-B alarm source: an intrinsic ChangeOfState(bool) algorithm routed to Notification Class 1 (Crimson); it is not writable through any API in this stack (a real installation drives it from hardware), so this example simulates it on a 30-second timer rather than a key or a write - see the file header comment in main.cpp. Optional properties Assigned_Landing_Calls, Registered_Car_Call and Landing_Door_Status are enabled and each exercises one of the four F-ELEVATOR list/sequence callbacks with one demonstration entry

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Elevator_Group | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Installation_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |

### Escalator 1 "Mint" - F-ELEVATOR. Not a member of Maroon (a group of Lifts, isGroupOfLifts=true) - added with the spec's 'no reference' sentinel (4194303) for Elevator_Group, as clause 12.60.6 allows. Required Operation_Direction (no stack default), Passenger_Alarm and Out_Of_Service are served by the app; the optional Power_Mode and Escalator_Mode are enabled and served too, for a fuller demonstration. Status_Flags/Elevator_Group/Group_ID/Installation_ID are stack-stored, same reasoning as Mauve's

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Elevator_Group | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Installation_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Power_Mode *(optional, enabled)* | Boolean | stack default (Generic Boolean default: `false`) | no |
| Operation_Direction | BACnetEscalatorOperationDirection | app | no |
| Escalator_Mode *(optional, enabled)* | BACnetEscalatorMode | stack default (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Passenger_Alarm | Boolean | app | no |

### Positive Integer Value 1 "Turquoise" - not one of the profile's named objects - added explicitly by this application, BEFORE Elevator Group 1 (Maroon), as the object Maroon's Machine_Room_ID references. BACnetStack_AddElevatorGroupObject's own doc comment reads as though the stack creates this object automatically if missing; verified against the running stack, it does not - AddElevatorGroupObject fails outright ('Create the Positive Integer Value object before adding the Elevator Group object') unless the application adds it first. Colour 'Turquoise' per docs/colour-table.md's global positive_integer_value mapping; Present_Value (required, no stack default) is served as an arbitrary plausible room id. Units defaults to no-units (accepted) since a machine-room identifier has none; Out_Of_Service is optional and left at the generic default

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Units | BACnetEngineeringUnits | stack default, accepted (`BACnetEngineeringUnits::noUnits`) | no |

### Notification Class 1 "Crimson" - AE-N-I-B / AE-ACK-B / AE-INFO-B. Priority, Ack_Required and Recipient_List are NOT stack DEFAULTS - they are genuinely populated by BACnetStack_AddNotificationClassObject (Priority, Ack_Required) and BACnetStack_AddRecipientToNotificationClass (Recipient_List) at start-up; marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API. Unlike B-AAC (AE-CRL-B), Recipient_List is NOT made writable here - AE-CRL-B is not a required BIBB for this profile

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Priority | BACnetARRAY[3] of Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Ack_Required | BACnetEventTransitionBits | stack default, accepted (Generic BitString default: empty bitstring (zero bits - NOT ) | no |
| Recipient_List | BACnetLIST of BACnetDestination | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |

### Event Log 1 "Beige" - AE-EL-I-B (F-EVENTLOG, NEW vs B-EC; this series' canonical pattern is B-ALSC-CPP). Enable/Buffer_Size/Record_Count/Total_Record_Count/Log_Buffer/Status_Flags/Object_Identifier/Object_Type/Property_List are NOT stack defaults in the generic sense - they are genuinely held and served by the stack's own Event Log engine (BACnetStack_AddEventLogObject), answered from stack storage BEFORE this file's Get*/Set* callbacks are ever consulted; marked accepted only because property-profile-reference.md's generic table does not know about this object-specific engine. VERIFIED over the wire with a live bacpypes3 client against THIS repository's own build: Object_Name (undefined, the stack's generic non-Device default - no callback serves it), Record_Count and Total_Record_Count (both 0) all read correctly. VERIFIED DEFECT, independently reproduced in this repository (chipkin/cas-bacnet-stack#2045, first found by B-ACC-CPP): adding this object alone causes a continuous, non-fatal 'Failed to set the date'/'Failed to set the time' internal log flood starting from the very first BACnetStack_Tick() - 200 occurrences of each line observed in a 5-second smoke test window. NOTE: B-ALSC-CPP's own task (same object, same BACnetStack_AddEventLogObject call, same stack pin) did NOT observe this flood and its TODO.md/objects.json make no mention of it - the two repositories' object combinations differ (this one also adds Schedule/Calendar/File/Backup-Restore/DM-OCD-B), so the trigger is not 'AddEventLogObject alone' as B-ACC's bisection concluded; it reproduces in at least one combination the same as B-ACC's (Event Log + Schedule/Calendar + File/Backup-Restore) and did not reproduce in B-ALSC's narrower one (Event Log + Schedule/Calendar, no File/Backup-Restore). Flagged in TODO.md with this comparison rather than re-asserting B-ACC's isolation claim unverified. The device otherwise keeps answering Who-Is/ReadProperty correctly throughout the flood (wire-verified)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Enable | Boolean | stack default, accepted (Generic Boolean default: `false`) | yes |

### Schedule 1 "Saffron" - SCHED-I-B (F-SCHED-I, NEW vs B-EC; canonical pattern from B-AAC-CPP, copied via B-ALSC-CPP's more recent template). Present_Value, Effective_Period, Schedule_Default, List_Of_Object_Property_References, Priority_For_Writing and Status_Flags are NOT stack defaults - they are genuinely held and served by the stack's Schedule engine (BACnetStack_AddScheduleObject plus the BACnetStack_SetSchedule*/AddSchedule* configuration calls in main.cpp); it writes Analog Output 1 (Chartreuse) Present_Value at priority 8. One weekly transition (Monday 08:00) and one calendar-date exception (2026-12-25, inline calendar-entry form) are seeded at start-up. VERIFIED over the wire with a live bacpypes3 client: Object_Name (undefined) and Priority_For_Writing (8) both read back correctly

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Any | stack default, accepted (Stack-generated if commandable (resolves the priority array)) | no |
| Effective_Period | BACnetDateRange | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Schedule_Default | Any | stack | no |
| List_Of_Object_Property_References | BACnetLIST of BACnetDeviceObjectPropertyReference | stack | no |
| Priority_For_Writing | Unsigned(1..16) | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | app | no |
| Out_Of_Service | Boolean | app | no |

### Calendar 1 "Cream" - Exists for SCHED-I-B completeness alongside Saffron's exception, but its Date_List cannot be populated through the customer API (cas-bacnet-stack issue #963 - no read path for a Calendar object's Date_List; the only generic constructed-property callback is test-tool-only, same gap B-AAC/B-ALSC/B-ACC all document). Present_Value therefore always answers false rather than evaluating a Date_List that is never populated. VERIFIED over the wire: Present_Value read back as false (0)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Boolean | app | no |
| Date_List | BACnetLIST of BACnetCalendarEntry | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |

### File 1 "Ivory" - DM-BR-B (F-BACKUP, NEW vs B-EC; canonical pattern from B-ACC-CPP) backup/restore payload carrier - stream access, in-memory 4KB buffer via BACnetStack_RegisterCallbackReadFile/WriteFile. File_Type/File_Size/Modification_Date/Archive/Read_Only all have NO stack default per property-profile-reference.md and are all served here. VERIFIED over the wire: File_Size read back as 0 (empty file) - this repository's own build did NOT reproduce B-ACC-CPP's documented File_Size read-access-denied finding (TODO.md #5 / issue #2046); see TODO.md for the comparison. AtomicReadFile/AtomicWriteFile round-trip and the restore half of the backup/restore cycle were NOT exercised over the wire this session (carrying forward B-ACC's documented, unresolved gap there rather than re-testing it) - see TODO.md

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| File_Type | CharacterString | app | no |
| File_Size | Unsigned | app | no |
| Modification_Date | BACnetDateTime | app | no |
| Archive | Boolean | app | yes |
| Read_Only | Boolean | app | no |
| File_Access_Method | BACnetFileAccessMethod | stack | no |

### Analog Value 2 "(created at runtime via DM-OCD-B; not present at start-up)" - DM-OCD-B (F-OCD, NEW vs B-EC and the headline addition of this repository - canonical for the whole series). Analog Value is the ONLY creatable object type (BACnetStack_SetObjectTypeCreatable); no Analog Value instance exists until a client creates one with CreateObject. This row documents instance 2 - the object created and verified in this session's live wire test, then deleted - not a base object present at start-up (it does not appear in the base Object_List). Present_Value is served entirely by this file's g_createdAnalogValues table (no stack default for a non-commandable Analog Value's Present_Value) and made writable at the object-TYPE level (BACnetStack_SetPropertyByObjectTypeWritable, since there is no per-instance object to configure before one is created). Object_Name/Units/Out_Of_Service/Event_State/Status_Flags are accepted at their generic stack defaults (Object_Name reads 'undefined', Units 'no-units') - this example does not attempt to let a client name or configure a newly-created object beyond Present_Value. VERIFIED over the wire with a live bacpypes3 client against this repository's own build: CreateObject(analog-value, 2) accepted (CreateObjectACK); the object then appears in Object_List; ReadProperty of Present_Value (0.0) and Object_Name (undefined) both succeed; WriteProperty of Present_Value to 42.5 accepted and reads back correctly; DeleteObject(analog-value, 2) accepted (SimpleACK); a subsequent ReadProperty of the deleted instance correctly answers Error(object: unknown-object); CreateObject(binary-input, 5) - a non-creatable type - is correctly rejected with a CreateObjectError, never reaching this file's CreateObject callback in a way that allocates anything

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | stack default, accepted (the literal string `"undefined"` (non-Device objects only)) | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |
| Units | BACnetEngineeringUnits | stack default, accepted (`BACnetEngineeringUnits::noUnits`) | no |

<!-- OBJECTS-PROPERTIES:END -->

## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
The CAS BACnet Stack supports every standardized device profile in ASHRAE 135-2024 Annex L. One example repository per profile shows how. ✅ = the required BIBB (service) is supported by the CAS BACnet Stack; the **Example** column is the state of that profile's tutorial repository.

### Controllers (Annex L.4)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) ✅ | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) ✅ · [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-CRL-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) 📝 | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-RPM-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-CRL-B · ✅ SCHED-E-B · ✅ T-VMT-I-B · ✅ T-ATR-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Life safety controllers (Annex L.5)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ AE-LS-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ AE-LS-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |

### Access control controllers (Annex L.6)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-ACUC-B · ✅ DS-ACSC-B · ✅ AE-AC-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) 📝 | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-RPM-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-A · ✅ DS-COV-B · ✅ DS-ACAD-A · ☐ DS-ACCDI-A · ✅ DS-ACUC-B · ✅ DS-ACSC-B · ✅ AE-AC-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Lighting controllers (Annex L.11)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-LO-B / DS-BLO-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WG-E-B · ✅ DS-ALO-A · ✅ SCHED-E-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |

### Elevator controllers (Annex L.13)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-OCD-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Authentication and authorization (Annex L.14)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) 🚧 | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ AA-AS-B |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ NM-BBMDC-B |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-ACAD-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-COV-B · ✅ DS-ACCDI-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-A · ✅ DM-DOB-B · ☐ DM-LM-B · ✅ NM-RC-B |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ GW-EO-B / GW-VN-B |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DAB-B |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) 📝 | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ NM-SCH-B |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12) — client-side profiles

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) ✅ | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-V-A · ✅ DS-M-A · ✅ AE-N-A · ✅ AE-VN-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-OWS** Operator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-V-A · ✅ DS-M-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-VM-A · ✅ AE-VN-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-MTS-A |
| **B-AWS** Advanced Operator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-AV-A · ✅ DS-AM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-AVM-A · ✅ AE-AVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ DM-DDA-A · ✅ NM-CC-A · ✅ AR-AVM-A |
| **B-XAWS** Extended Advanced Operator Workstation | planned | ✅ union of B-AWS + B-AACWS + B-ALWS + B-AEWS |
| **B-LSAP** Life Safety Annunciator Panel | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LSV-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-LSVN-A |
| **B-LSWS** Life Safety Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LSV-A · ✅ DS-LSM-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-LSVM-A · ✅ AE-LSAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-ALSWS** Advanced Life Safety Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LSAV-A · ✅ DS-LSAM-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-LSAVM-A · ✅ AE-LSAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ AR-AVM-A |
| **B-ACSD** Access Control Security Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACV-A · ✅ DS-ACM-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-MTS-A |
| **B-ACWS** Access Control Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACAV-A · ✅ DS-ACM-A · ✅ DS-ACUC-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACVM-A · ✅ AE-ACAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-AACWS** Advanced Access Control Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACAV-A · ✅ DS-ACAM-A · ✅ DS-ACUC-A · ✅ DS-ACSC-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACAVM-A · ✅ AE-ACAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ AR-AVM-A |
| **B-LOD** Lighting Operator Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LV-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ALWS** Advanced Lighting Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LAV-A · ✅ DS-LAM-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-AVM-A · ✅ AE-AVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-LCS** Lighting Control Station | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LO-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-ALCS** Advanced Lighting Control Station | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ SCHED-E-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-ED** Elevator Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-EV-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-EVN-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-EWS** Elevator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-COVM-A · ✅ DS-EV-A · ✅ DS-EM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-EVM-A · ✅ AE-EAVN-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A |
| **B-AEWS** Advanced Elevator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-COVM-A · ✅ DS-EAV-A · ✅ DS-EAM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-EAVM-A · ✅ AE-EAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |

Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
<!-- PROFILE-TABLE:END -->

## Footprint

Release-build sizes and start-up timing, from the latest tagged release's CI
run (`metrics-windows.json` / `metrics-linux.json`), both built with
`CAS_BACNET_STACK_LINK=STATIC`:

<!-- METRICS -->
| Platform | Binary | Size | SHA-256 (prefix) | Start-up to `ready` | Stack commit | Link mode | Compiler |
|---|---|---|---|---|---|---|---|
| Windows x64 (windows-2022) | `BACnetExampleBAEC.exe` | 3,439,104 bytes (~3.3 MiB) | `dea17db7c0f9a32c` | 242 ms | `abd4cee1` | STATIC | Visual Studio 17 2022 |
| Linux x64 (ubuntu-latest) | `BACnetExampleBAEC` | 69,928 bytes (~68 KiB) | `64bec198dc53a056` | 114 ms | `abd4cee1` | STATIC | `/usr/bin/c++` |

From release [v1.0.0](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP/releases/tag/v1.0.0) (`metrics-windows.json` / `metrics-linux.json`).

## References

- **ANSI/ASHRAE 135** - object model (Clause 12), alarming/events (Clause 13),
  services (Clause 16), device profiles (Annex L).
- **CAS BACnet Stack** - <https://store.chipkin.com/services/stacks/bacnet-stack>.
- **B-EC example** (the seed this repository extends) -
  <https://github.com/chipkin/BACnetProfileExample-B-EC-CPP>.
- **B-ALSC example** (the AE-EL-I-B / Event Log pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP>.
- **B-AAC example** (the SCHED-I-B / Schedule pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP>.
- **B-ACC example** (the DM-BR-B / Backup-Restore pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP>.
- **[CHANGELOG.md](CHANGELOG.md)**, **[AGENTS.md](AGENTS.md)**,
  **[`common/README.md`](common/README.md)**.

## Use this in your own project

Self-contained: clone (with the submodule) and build, then copy what you need. The
example source is **CC0-1.0** (public domain). The CAS BACnet Stack is a separate,
commercially licensed product not covered by CC0.
