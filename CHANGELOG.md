# Changelog

All notable changes to this project are documented in this file.

## [1.0.0] - unreleased

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
- **Verified over the wire with a live `bacpypes3` client against this
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
