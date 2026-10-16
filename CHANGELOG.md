# Changelog

All notable changes to this project are documented in this file.

## [1.1.0] - unreleased

### Fixed

- **DM-BR-B could never start.** The ReinitializeDevice callback refused every
  state but COLDSTART and WARMSTART, and the stack drives a backup or restore
  only after that callback accepts STARTBACKUP .. ABORTRESTORE
  (`BACnetStack_RegisterCallbackReinitializeDevice`). It now accepts them.
  Checked by hand on Windows and Linux: STARTBACKUP, AtomicReadFile, ENDBACKUP,
  STARTRESTORE, AtomicWriteFile, ENDRESTORE all acknowledged, and File 1 reads
  back the restored content. The README ticked DM-BR-B before this worked.
- **AE-EL-I-B: ReadRange was not enabled**, so the Event Log's `Log_Buffer`
  could not be read at all (Reject unrecognized-service). AE-EL-I-B executes
  ReadRange (135-2024 Annex K.2.21); it is now enabled.
- **File 1 (Ivory) `File_Type` and `Modification_Date`**, both required with no
  stack default, answered `read-access-denied`: nothing served them (the PICS
  said they were served). `File_Type` is now `application/octet-stream`;
  `Modification_Date` is stamped at start-up and by every WriteFile.
- **Elevator Group 1 (Maroon) `Landing_Calls` and `Landing_Call_Control`**
  answered `read-access-denied` - the same two callback defects as B-EC-CPP
  1.1.0: an empty `Landing_Calls` is now answered by returning `false`, and
  `Landing_Call_Control` reads floor 0, direction `none` until a call is
  written. A raw-encoded write then reads back unchanged (by hand).
- All five were found by the new discovery Wire Test (22/23 before, 24/24
  after), except the backup states and ReadRange, found while re-checking
  the TODO.md blockers at the new pin.

### Added

- **Wire Tests** (`tests/`, README "Wire Tests"): `tests/discovery_test.py`,
  B-SS-CPP's discovery test with this example's object list, plus B-ACC-CPP's
  rule that a log object's `Log_Buffer` must be *refused* by ReadProperty (cl.
  12.27.13, 15.5.1.3.1). `tests/run_wire_tests.py` runs it; CI runs it on Linux
  after the smoke test. 24/24 on Windows and Linux.
- `wiretest/` 0.5.0, the series' shared Wiretest Harness, vendored verbatim.

### Changed

- **`common/` 3.3.0 -> 3.4.0** and **`wiretest/` 0.5.0 -> 0.6.0**, both copied
  verbatim (`common/` from B-SS-CPP `5d78299`, `wiretest/` from B-LS-CPP). With
  `common/` 3.4.0 a broadcast the stack addresses to this socket's own address
  goes to the subnet broadcast instead; `wiretest/` 0.6.0 only adds helpers.
  `tests/discovery_test.py` now sends its Who-Is a second time, with a visible
  `[RETRY]` line, if no I-Am comes back in 3 s: Who-Is is unconfirmed and the
  example's UDP receive can drop a datagram on Windows. Discovery results are
  unchanged: 24/24 on Windows and Linux, no `[RETRY]`.
- **Stack Pin** `6.x` @ `abd4cee1` (6.0.21) -> `6.x` @ `64b2cfac5` (6.0.23). No
  API change needed.
- **`common/` 3.0.0 -> 3.3.0** (B-SS-CPP `b640a81`, verbatim).
- **Protocol_Revision 24 -> 30** (the stack's default at the new pin).
- **Segmentation is now supported** (`segmented-both`, `Max_Segments_Accepted`
  16, `APDU_Segment_Timeout` 5000 ms): the stack's customer build compiles
  segmentation in from this pin (stack #2992). `docs/PICS.md` section 5 updated.
- **TODO.md rewritten against the new pin.** #2045 (Event Log log flood),
  #2151/#2152 (File_Size, AtomicReadFile during a backup) and #963 (Calendar)
  are closed and removed. Three stack defects found at this pin are recorded,
  not filed: the Event Log's required `Total_Record_Count` is missing, an Event
  Log starts disabled with no customer call to enable it, and the device logs
  its own broadcast notification twice.
- **README, PICS and comments are honest**: a status note (BIBB-conformant, not
  Profile-functional: landing calls are stored, nothing moves the car; the
  Event Log needs a client to enable it); the object tree no longer says Mauve
  has "car position/doors"; the "captures an Event Log" claim is qualified;
  the verification list says what is a Wire Test and what was checked by hand;
  AE-EL-I-B is "Event Log - Internal" (Annex K.2.21), not "Enrollment Log";
  the PICS cites Annex L.13.2/L.13.3 for B-EC/B-EM, not L.14/L.13.
- README, AGENTS.md and PICS version lines: stack 6.0.23, `common/` 3.3.0,
  Protocol_Revision 30 (the README still said example v1.0.0 and `common/`
  2.5.0). The series profile table is re-synced. APP_VERSION 1.0.2 -> 1.1.0.

## [1.0.2] - unreleased

### Changed

- **Device renamed from the series' colour placeholder "Rainbow" to "Chipkin
  Example B-AEC"** so devices from different examples in the series are
  distinguishable from each other on the same BACnet network - every example
  previously announced the identical Object_Name "Rainbow", which made two
  examples on one subnet indistinguishable by name. Sub-object names (Analog
  Input 1 "Bronze", etc.) are unchanged - only the Device object's name
  changed. `docs/colour-table.md` (series root) updated to match. APP_VERSION
  bumped 1.0.1 -> 1.0.2.

## [Unreleased]

### Fixed

- **`Application_Software_Version` (12) and `Firmware_Revision` (44) were
  hardcoded and stale** - the same bug found and fixed in the sibling
  BACnetProfileExample-B-SCHUB-CPP example via a real device read (BACnet
  Explorer): `Application_Software_Version` reported a hardcoded `"1.0.0"`
  regardless of the actual build, and `Firmware_Revision` was the same stale
  `"1.0.0"` constant - it was never meant to be this example's own version at
  all, it names the underlying platform. Fixed: `Application_Software_Version`
  now reads `APP_VERSION` directly (one source of truth, can't drift from
  `--version`'s own banner again). `Firmware_Revision` is now built at
  runtime from the CAS BACnet Stack's own `BACnetStack_GetAPIMajorVersion()`/
  `GetAPIMinorVersion()`/`GetAPIPatchVersion()`/`GetAPIBuildVersion()` (the
  same 4 calls `common/CASExampleHelper.cpp`'s `PrintVersion()` already uses
  for the startup banner), populated once right after `LoadBACnetFunctions()`
  succeeds. APP_VERSION bumped 1.0.0 -> 1.0.1.

### Changed

- Restructured the documentation to match the series pattern set by
  `BACnetProfileExample-B-SS-CPP`: `README.md` is cut down to this example
  only (series framing, the generic profile explanation, the "What the
  profile requires" prose, "Before you ship", "Get the code", "Link mode",
  "Troubleshooting", "Extending the example", the "Objects and properties"
  table, and the CC0 paragraph all moved out or removed); the long-form
  material moved to a new `TUTORIAL.md` (extending the example, what each
  object type needs served, the DM-OCD-B CreateObject/DeleteObject cycle
  traced end to end, Reviewing your device, Troubleshooting - carrying every
  silent-failure and conformance warning over precisely, including the Event
  Log flood and File_Size discrepancy notes); a new `docs/PICS.md` (ANSI/ASHRAE
  135 Annex A shape) replaces the README's inline objects-and-properties block.
- `docs/objects.json` gained a `Device` entry (previously omitted from the
  generated tables); `docs/PICS.md`'s objects-and-properties block was
  regenerated from it with `tools/gen-objects-properties.py` - 0 rows flagged
  `⚠`.
- **Build switched from a prebuilt STATIC library to the adapter's default
  SOURCE mode**, matching the rest of the series: `cmake -B build -S .` /
  `cmake --build build --config Release`, no `tools/build-stack-static.sh`
  step and no `-DCAS_BACNET_STACK_LINK=STATIC` flag. `CMakeLists.txt`'s header
  comment, `AGENTS.md`'s build section, and `.github/workflows/release.yml`
  (dropped the static-library cache/build steps and the matrix `lib:` entries,
  configure with no link-mode flag, the link-mode assertion now checks
  `SOURCE`, `metrics-*.json` now records `"link_mode": "SOURCE"`, and the
  packaged release artifact now includes `TUTORIAL.md` and `docs/PICS.md`)
  were all updated to match. The v1.0.0 footprint numbers in `README.md` were
  measured from the old STATIC build; the table is kept with a note that the
  next release refreshes them under the SOURCE build documented here.
- `main.cpp`'s `CHANGE ALL OF THIS BEFORE YOU SHIP` block now carries a
  per-field comment for every constant, including the `DEVICE_NAME` uniqueness
  warning (must be unique across the BACnet internetwork; a real product needs
  it per-unit configurable, not compile-time) that previously lived only in
  the README's "Before you ship" table.
- The series `PROFILE-TABLE` block in `README.md` re-synced with
  `tools/sync-profile-table.sh BACnetProfileExample-B-AEC-CPP`.

## [1.0.0] - 2026-09-15

### Added

- Initial B-AEC (Advanced Elevator Controller) example, built by seeding from
  `BACnetProfileExample-B-EC-CPP` (Wave 3, the Elevator Controller) and adding
  logging, scheduling, dynamic object creation and backup/restore.
- CAS BACnet Stack pinned to `6.x` @ `abd4cee1c7f28ca8e1af4720849c4081082bbe82`
  (reports 6.0.21), linked as a **STATIC** library
  (`CAS_BACNET_STACK_LINK=STATIC`; built first by `tools/build-stack-static.sh`
  from the stack's own project files - see README "Link mode").
- `common/` vendored from `BACnetProfileExample-B-SS-CPP` (series source of
  truth) at v2.5.0, copied verbatim.
- Everything B-EC already has: DS-RP-B, DS-RPM-B, DS-WP-B, DS-WPM-B, DS-COV-B,
  DS-COVM-B, AE-N-I-B, AE-ACK-B, AE-INFO-B, DM-DDB-A, DM-DDB-B, DM-DOB-B,
  DM-DCC-B, DM-TS-B, DM-RD-B, the base sensors, three commandable outputs
  (F-OUTPUTS), Network Port, Elevator Group/Lift/Escalator family,
  Notification Class, and the intrinsic ChangeOfState alarm on the Lift's
  Passenger_Alarm.
- **DM-OCD-B** (this repository's headline addition; **canonical for F-OCD**
  across the series): `BACnetStack_SetObjectTypeCreatable(analog-value, true)`
  marks Analog Value the ONLY creatable object type;
  `BACnetStack_RegisterCallbackCreateObject`/`RegisterCallbackDeleteObject`
  let a client create and delete instances at runtime, tracked in a small
  host-side table (`g_createdAnalogValues`) since `CallbackCreateObject` fires
  before the object exists in the stack's own database. A created instance's
  `Present_Value` is made writable at the object-TYPE level
  (`BACnetStack_SetPropertyByObjectTypeWritable`). Services CreateObject (10)
  and DeleteObject (11) enabled - numbers re-verified against
  `BACnetServicesSupported.h` at the pin, not assumed.
- **AE-EL-I-B / F-EVENTLOG** (canonical pattern from
  `BACnetProfileExample-B-ALSC-CPP`, copied byte-for-byte): Event Log 1
  "Beige" (`BACnetStack_AddEventLogObject`, buffer 100). `Log_Enable` writable.
- **SCHED-I-B / F-SCHED-I** (canonical pattern from
  `BACnetProfileExample-B-AAC-CPP`, copied via B-ALSC-CPP's template): Schedule
  1 "Saffron" + Calendar 1 "Cream". Saffron writes Analog Output 1
  "Chartreuse" `Present_Value` at priority 8 (one weekly Monday 08:00
  transition, one 2026-12-25 calendar-date exception,
  `Schedule_Default` otherwise). The `s` key (`KeyCommand::DemoAdvance`,
  claimed by B-AAC, `common/` 2.1.0+) adds a transition for right now.
- **DM-BR-B / F-BACKUP** (canonical pattern from
  `BACnetProfileExample-B-ACC-CPP`, copied byte-for-byte): File 1 "Ivory"
  (stream-access, in-memory 4KB backup/restore payload carrier),
  `BACnetStack_SetBackupAndRestoreEnabled` plus the four Prepare/Complete
  Backup/Restore callbacks; `Archive` writable.
- `docs/objects.json` extended for Beige, Saffron, Cream, Ivory, and the
  dynamically-creatable Analog Value; the generated "Objects and properties"
  README block regenerated (0 ⚠ rows).
- The series profile-table block and footprint placeholder in README.

### Notes

- Every BIBB this profile requires is implemented against the pinned stack -
  including DM-OCD-B, the headline requirement, which is fully implemented
  and wire-verified (see README "Verify" and `docs/objects.json`).
- **Verified over the wire with a live BACnet client against this
  repository's own build**: DM-OCD-B's full CreateObject/WriteProperty/
  DeleteObject/rejection cycle; Event Log's `Object_Name`/`Record_Count`/
  `Total_Record_Count`; Schedule's `Priority_For_Writing`; Calendar's
  `Present_Value`; File's `File_Size`. F-OUTPUTS/F-ELEVATOR/F-COVM/alarming/
  DM-TS-B/DM-RD-B/DM-DDB-A are unchanged code inherited from B-EC-CPP, already
  wire-verified there, and were not independently re-run against this
  repository's build this session.
- **Known stack defect, independently reproduced here**
  ([chipkin/cas-bacnet-stack#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)):
  adding Event Log 1 ("Beige") causes a continuous, non-fatal internal
  `Failed to set the date`/`Failed to set the time` log flood from the first
  `Tick()`. First found by `BACnetProfileExample-B-ACC-CPP`; notably, this
  repository's sibling `BACnetProfileExample-B-ALSC-CPP` adds the identical
  object and did **not** observe the flood - see `TODO.md` for the
  object-combination comparison. The device keeps answering BACnet requests
  correctly throughout.
- **Genuine, new finding**: this repository's build did **not** reproduce
  `BACnetProfileExample-B-ACC-CPP`'s documented `File_Size` read-access-denied
  gap (part of issue #2046) - `File_Size` read back correctly (0) here. See
  `TODO.md`.
- DM-BR-B's restore half and a concurrent AtomicReadFile are carried forward
  as documented, unresolved gaps from `BACnetProfileExample-B-ACC-CPP`
  (issue #2046), not re-tested this session.
- Calendar 1 ("Cream")'s `Date_List` cannot be populated through the customer
  API (stack issue #963), the same inherited gap B-AAC/B-ALSC/B-ACC document;
  Saffron's one exception uses the inline calendar-entry form instead.
