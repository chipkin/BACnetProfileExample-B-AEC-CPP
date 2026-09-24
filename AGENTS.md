# AGENTS.md

Guidance for AI coding agents working in this repository. See
<https://agents.md/> for the format. Human contributors should read
[README.md](README.md) first, then [TUTORIAL.md](TUTORIAL.md).

## What this project is

A **tutorial** C++ example that implements the BACnet **B-AEC (Advanced
Elevator Controller)** profile as fully as the standard CAS BACnet Stack
supports. It is one of a series - one git repo per BACnet profile. B-AEC
seeded from [B-EC](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP)
(the Elevator Controller) and adds four features: **DM-OCD-B**
(CreateObject/DeleteObject - the profile's headline addition; **this
repository is canonical for F-OCD**), **AE-EL-I-B** (Event Log, canonical
pattern from B-ALSC-CPP), **SCHED-I-B** (Schedule/Calendar, canonical pattern
from B-AAC-CPP), and **DM-BR-B** (Backup/Restore, canonical pattern from
B-ACC-CPP). Everything B-EC has (DS-RP-B/RPM-B/WP-B/WPM-B, COV/COV-Multiple,
intrinsic alarming, DM-DCC-B/DDB-A/TS-B/RD-B, F-OUTPUTS, the elevator object
family) is unchanged. The top priority is that the code reads like a tutorial
a customer can learn from and copy-paste. Favour clarity over cleverness.

## Layout

This repository is self-contained:

- `main.cpp` - the example device.
- `common/` - the shared helper (vendored).
- `README.md` - what this example is. Keep it short and about THIS example only.
- `TUTORIAL.md` - how to extend and review the example. Long-form material that
  would bloat the README belongs here.
- `docs/PICS.md` - the Protocol Implementation Conformance Statement. Its
  objects-and-properties section is GENERATED from `docs/objects.json`; do not
  hand-edit between the `OBJECTS-PROPERTIES` markers.
- `docs/objects.json` - the input to that generator. Update it in the same change
  as any `main.cpp` change that adds an object or a `GetProperty*` branch.
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack** as a git submodule
  (private; compiled from source). After cloning, run
  `git submodule update --init --recursive`.

The `PROFILE-TABLE` block in README.md is also generated, from the example-series
repository's `docs/profile-table.md`. Edit it there, not here.

## Build

Plain CMake, identical on every platform, in the adapter's default SOURCE mode
(the stack's sources are compiled into the executable - no prebuilt library, no
DLL, no per-platform pre-step):

```bash
git submodule update --init --recursive   # once, if not cloned with --recursive
cmake -B build -S .
cmake --build build --config Release
```

The first build compiles the whole stack (~600 files) and takes a few minutes;
rebuilds after that are incremental and fast. Use `-D CAS_STACK_DIR=...` only if
your stack lives outside the bundled submodule. Do not reintroduce a link-mode
flag or a series-root build script into the documented build: a customer
downloads this repository on its own and must be able to build it with the two
commands above.

## Run

```bash
./build/BACnetExampleBAEC [--port 47808] [--deviceID 389014]   # Linux/macOS
.\build\Release\BACnetExampleBAEC.exe [--port 47808] [--deviceID 389014]   # Windows
```

Interactive keys while running: `h` help, `q` quit, up/down nudge Analog Input
1 (Bronze) - also the F-COVM demonstration - `d` broadcasts a demo Who-Is
(DM-DDB-A), and `s` adds a Weekly_Schedule transition for right now (SCHED-I-B
demo). CreateObject/DeleteObject (DM-OCD-B) and WriteProperty are exercised
over the wire (no interactive key commands either - use a BACnet client).

## Conventions

- Device is named "Chipkin Example B-AEC"; objects use the series' colour names; vendor id 389.
- **F-OUTPUTS is the canonical pattern from `BACnetProfileExample-B-SA-CPP`,
  copied verbatim** - the `Commandable` struct, `CommandWrite`/`CommandRelinquish`,
  `ReadPrioritySlot`, `GetCommandable`, `BACNET_PRIORITY_ARRAY_SIZE`, and the
  Set* callbacks (`SetPropertyReal`, `SetPropertyEnumerated`,
  `SetPropertyUnsignedInteger`, `SetPropertyNull`). If you extend this pattern
  to a new object type, walk EVERY Get callback branch for that type - see
  "the Relinquish_Default bug" below for what happens if you miss one.
- F-ELEVATOR: unchanged from B-EM's `AddElevatorGroupObject` /
  `AddLiftOrEscalatorObject` pattern - see B-EM's AGENTS.md for the full
  writeup. The one genuinely new piece:
  **`RegisterCallbackSetElevatorGroupLandingCallControl` IS registered here**
  (B-EM deliberately does not register it). The queued call is stored in
  `g_queuedLandingCall` and read back through
  `GetListElevatorGroupLandingCallStatus`, which now branches on
  `propertyIdentifier` (`Landing_Call_Control` vs `Landing_Calls`) - B-EM's
  read-only version ignored that parameter because it only ever answered
  `Landing_Calls`.
- F-TIMESYNC is the canonical pattern from `BACnetProfileExample-B-LD-CPP`,
  copied verbatim - `SyncedDateTime`, `InitSyncedDateTimeFromHost`,
  `SetSystemTime`, `GetPropertyDate`/`GetPropertyTime`. Only
  `SERVICE_TIME_SYNCHRONIZATION` is enabled, not the UTC variant - this
  profile allows either (DM-TS-B or DM-UTC-B); B-LD makes the same choice.
- F-REINIT is the pattern shared by B-AAC/B-ASC/B-LSC - `PasswordAccepted`,
  `ReinitializeDevice`, and the main-loop's deferred-restart block using
  `CASExampleHelper::RequestRestart`/`RestartDue`. **Never act on a restart
  inside the `ReinitializeDevice` callback itself** - the SimpleAck has not
  reached the wire yet at that point; always defer to the main loop.
- **The Relinquish_Default bug** (fixed before this repository's first
  release, kept here as a warning): when adapting B-SA's Get-callback pattern
  to a new commandable object, EVERY branch needs BOTH the `ReadPrioritySlot`
  case AND the `PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT` case. Binary Output's
  `GetPropertyEnumerated` branch was written with the array-slot case and the
  `Polarity` case but the `Relinquish_Default` case was omitted - the build
  was clean (zero warnings) and `Present_Value` reads for the OTHER two
  outputs (Analog, Multi-State) worked fine, masking the bug until a wire
  test against Binary Output specifically returned `read-access-denied` on
  BOTH `Relinquish_Default` and `Present_Value` (the stack cannot resolve a
  commandable object's `Present_Value` from an all-null Priority_Array
  without a servable `Relinquish_Default`). When you copy this pattern again,
  diff every Get callback branch against B-SA's three (Real/Enumerated/
  UnsignedInteger), property by property, not just by eye.
- `docs/property-profile-reference.md`'s generated Lift table is **incomplete**
  relative to the required properties documented in
  `CASBACnetStackDLL.h`'s own comment above
  `BACnetStack_AddLiftOrEscalatorObject` (a documentation gap in the stack
  repo, unchanged from B-EM). Trust the DLL header's doc comment; `docs/objects.json`'s
  notes record exactly which properties this affects.
- F-COVM: unchanged from B-EM - `BACnetStack_SetPropertySubscribable` on two
  properties of Analog Input 1, plus `BACnetStack_SetCOVMultipleSettings`.
- Intrinsic alarming: unchanged from B-EM - see B-EM's AGENTS.md.
- DeviceCommunicationControl (DM-DCC-B): unchanged from B-EM. Shares
  `PasswordAccepted` with `ReinitializeDevice` in this repository (B-EM had
  its own inline password check since it had no `ReinitializeDevice`).
- Match the surrounding code style: `const`-correct parameters, check every stack
  return value, keep `main.cpp` linear and well-commented.
- **DM-OCD-B is canonical here (F-OCD)**: `g_createdAnalogValues` (declared
  near the F-OUTPUTS `Commandable` helpers, well before its first use) is a
  small fixed-size table (`MAX_CREATED_ANALOG_VALUES`, currently 8) tracking
  every runtime-created Analog Value. `CreateObject` fires BEFORE the stack's
  own object database knows about the object (see the doc comment on
  `BACnetStack_RegisterCallbackCreateObject` in `CASBACnetStackDLL.h`) - do
  NOT try to read the object back from the stack inside that callback. Only
  `Present_Value` is served for a created instance; every other required
  property (`Object_Name`, `Units`, `Out_Of_Service`, `Event_State`,
  `Status_Flags`) is deliberately left at its generic stack default (see
  `docs/objects.json`'s Analog Value entry). If you extend this pattern to a
  second creatable object type, remember `Present_Value`'s writability is set
  at the object-TYPE level (`BACnetStack_SetPropertyByObjectTypeWritable`),
  not per-instance - there is no instance to configure before one is created.
- **AE-EL-I-B is the canonical pattern from `BACnetProfileExample-B-ALSC-CPP`,
  copied byte-for-byte**: `BACnetStack_AddEventLogObject` - every property is
  stack-held, not served through a `Get*`/`Set*` callback.
  **`AddEventLogObject` reproduces a documented stack log flood in THIS
  repository** ([#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)) -
  see `TODO.md` before assuming it is silent.
- **SCHED-I-B is the canonical pattern from `BACnetProfileExample-B-AAC-CPP`**,
  copied via B-ALSC-CPP's template: `BACnetStack_AddScheduleObject` plus the
  `AddSchedule*`/`SetSchedule*` configuration calls - all stack-held, not
  callback-served. Cream's `Date_List` cannot be populated (stack issue #963);
  use the inline calendar-entry exception form, not a Calendar reference.
- **DM-BR-B is the canonical pattern from `BACnetProfileExample-B-ACC-CPP`**,
  copied byte-for-byte: File 1 "Ivory" (stream access) plus
  `ReadFile`/`WriteFile`/`PrepareBackup`/`CompleteBackup`/`PrepareRestore`/
  `CompleteRestore`. See `TODO.md` for the restore/AtomicReadFile gap carried
  forward from B-ACC-CPP.
- **Never edit `common/` in this repo alone** - it is a vendored copy shared by
  every example in the series, with its own version (`COMMON_VERSION`) and
  changelog (`common/CHANGELOG.md`). To change it: edit, bump the version, add
  a changelog entry, then re-copy `common/` into every example repository.
  This repository needed **no** `common/` change - the `d` key
  (`KeyCommand::DiscoverRemote`) and the `s` key (`KeyCommand::DemoAdvance`)
  were already claimed by B-LS-CPP and B-AAC-CPP respectively.

## How to verify a change

There are no unit tests; verification is behavioural:

1. Build, then run one instance on a clear UDP port.
2. With a BACnet client (e.g. the CAS BACnet Explorer, or `bacpypes3`/`BAC0`),
   send **Who-Is** and confirm **I-Am** from the device instance.
3. **ReadProperty** every required property of every object and confirm the
   values; confirm `Protocol_Revision` is 24 and `Object_List` lists all
   seventeen base objects.
4. **DM-OCD-B (this repository's headline feature)**: CreateObject
   `analog-value,2`, confirm it appears in `Object_List` and its
   `Present_Value` reads 0.0; WriteProperty `Present_Value` and confirm the
   readback; DeleteObject it and confirm a subsequent ReadProperty answers
   `unknown-object`; CreateObject a non-creatable type (e.g. `binary-input,5`)
   and confirm it is rejected.
5. **AE-EL-I-B**: ReadProperty Beige's `Object_Name`/`Record_Count`/
   `Total_Record_Count`; watch for the documented log-flood defect (`TODO.md`).
6. **SCHED-I-B**: ReadProperty Saffron's `Priority_For_Writing`; press `s` and
   confirm Chartreuse moves to `SCHEDULE_DEMO_VALUE` on the next tick.
7. **DM-BR-B**: ReadProperty Ivory's `File_Size`; ReinitializeDevice
   `startBackup`/`endBackup` and confirm `Backup_And_Restore_State`
   transitions (restore and AtomicReadFile/AtomicWriteFile carry a documented
   gap from B-ACC-CPP - see `TODO.md`).
8. **F-OUTPUTS**: WriteProperty each output's `Present_Value` at a priority,
   confirm the readback; relinquish (WriteProperty NULL) and confirm it falls
   back to `Relinquish_Default`; write an out-of-range value to Binary/Multi-
   State Output and confirm `value-out-of-range`.
9. **F-ELEVATOR**: ReadProperty every Elevator Group / Lift / Escalator
   property (unchanged checklist from B-EC); WriteProperty a landing call to
   `Landing_Call_Control` and confirm it reads back (this specific write was
   NOT independently wire-verified during B-EC's own implementation - see
   CHANGELOG.md - confirm it if you touch this code).
10. **DS-COVM-B / DS-COV-B, alarming, DM-TS-B, DM-RD-B, DM-DDB-A, device
    management**: unchanged from B-EC.
11. If you changed the objects or their properties, regenerate `docs/PICS.md`
    (`python tools/gen-objects-properties.py BACnetProfileExample-B-AEC-CPP` from
    the series root) and confirm no row comes out flagged with ⚠.

Verification is manual (no in-repo test suite ships). During development
`bacpypes3` was used against a running instance on a clear `--port` (mind the
SO_REUSEADDR gotcha - kill stale instances first).

## Releasing

Bump `APP_VERSION` in `main.cpp` and add an entry to [CHANGELOG.md](CHANGELOG.md),
then tag `vX.Y.Z`. The GitHub Actions workflow builds and publishes the release.

## License

See [LICENSE](LICENSE). The CAS BACnet Stack is a separate, commercially
licensed product and is not covered by it.
