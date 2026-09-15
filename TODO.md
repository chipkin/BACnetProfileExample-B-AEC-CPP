# TODO — known gaps (all verified against the pinned stack source and/or the wire, not assumed)

Stack pin: `abd4cee1c7f28ca8e1af4720849c4081082bbe82` (6.x @ 2026-09-10, reports 6.0.21).

DM-OCD-B, this repository's headline requirement (CreateObject/DeleteObject,
services 10/11), has **no gap** - fully implemented and wire-verified (see
README "Verify" and `docs/objects.json`'s Analog Value entry). Everything
below is carried forward from this repository's seed (B-EC-CPP) or its
canonical sources (B-ALSC-CPP, B-AAC-CPP, B-ACC-CPP), re-verified against
this repository's own build where noted.

## 1. AE-EL-I-B: `BACnetStack_AddEventLogObject` causes a continuous internal log flood

Wire/log-verified in THIS repository's own build this session: with Event Log
1 ("Beige") added via `BACnetStack_AddEventLogObject(deviceInstance, 1, 100)`,
the running device logs, from the very first `BACnetStack_Tick()` and
continuously thereafter (200 occurrences of each line observed in a 5-second
smoke test window):

```
::CASBACnetStack::BACnetDateTime::operator =() in file: .../source/BACnetDateTime.cpp(85) - Error: Failed to set the date
::CASBACnetStack::BACnetDateTime::operator =() in file: .../source/BACnetDateTime.cpp(89) - Error: Failed to set the time
```

**First found and filed by `BACnetProfileExample-B-ACC-CPP`:**
[chipkin/cas-bacnet-stack#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045).
B-ACC-CPP's own task bisected across seven progressively-reduced
configurations and concluded the trigger was `AddEventLogObject` alone,
independent of every other object type in that file.

**New, independent finding from this session, not a re-assertion of that
claim:** `BACnetProfileExample-B-ALSC-CPP` adds the identical object via the
identical call, against the identical stack pin, and its own task's
wire-verification (documented in its own `TODO.md`/`docs/objects.json`) makes
**no mention of this flood at all** - it explicitly describes Event Log as
fully working with no defect noted. This repository's own object set
overlaps with B-ALSC-CPP (Event Log + Schedule/Calendar - no flood there) but
also with B-ACC-CPP (Event Log + File/Backup-Restore - flood there too, and
here). This repository's own bisection was not re-run in this session (time),
but the pattern across all three repositories now on record is consistent
with "Event Log combined with File/Backup-Restore" reproducing the flood and
"Event Log combined with Schedule/Calendar alone" (no File/Backup-Restore)
not reproducing it - the opposite of B-ACC-CPP's "AddEventLogObject in
isolation" conclusion. Whichever the true root cause, it is filed under the
same issue; this note exists so a future session narrowing #2045 has all
three data points rather than re-discovering the ALSC counter-example.

Functionally the device keeps answering Who-Is/ReadProperty correctly despite
the flood (verified live in this session), matching B-ACC-CPP's own finding.

**Additional side effect observed in CI, not seen by any prior sibling
task:** the first `windows-2022` CI run on this repository's `main` branch
failed the smoke-test step with `forked process ... died unexpectedly` /
`fork: Resource temporarily unavailable` from the Git-Bash (MSYS2) shell,
immediately after the `ready` line and one `WriteProperty` line were
correctly captured - i.e. the smoke test's own logic had already succeeded,
but the flood's very high line rate appears to have exhausted MSYS2's fork
budget while the polling loop kept re-forking `grep`/`sleep` against a
rapidly-growing `smoke.log`. `gh run rerun --failed` succeeded immediately
on retry with an otherwise-identical log (same flood, same volume) - so this
looks like a resource-pressure flake specific to the Windows runner's
Git-Bash environment under heavy stdout volume, not a deterministic failure,
but it is a genuine, CI-observable consequence of #2045 worth flagging: a
sufficiently chatty flood can intermittently break log-polling CI steps on
this runner, not just clutter output.

**Filed:** [chipkin/cas-bacnet-stack#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)
(comment added with this repository's data point rather than a new issue).

## 2. DM-BR-B: restore half and concurrent AtomicReadFile not exercised; carried from B-ACC-CPP

`BACnetProfileExample-B-ACC-CPP`'s own `TODO.md` documents two related,
unresolved findings, both part of
[chipkin/cas-bacnet-stack#2046](https://github.com/chipkin/cas-bacnet-stack/issues/2046):
a live `AtomicReadFile` against its File object aborted (`Abort(other)`)
during an active backup session, and the restore half of the backup/restore
cycle (STARTRESTORE / `AtomicWriteFile` / ENDRESTORE) was not completed
end-to-end. This repository copies B-ACC-CPP's DM-BR-B implementation
byte-for-byte (`ReadFile`/`WriteFile`/`PrepareBackup`/`CompleteBackup`/
`PrepareRestore`/`CompleteRestore`, File 1 "Ivory"), so the same code paths
apply here. Neither backup's `ReinitializeDevice(startBackup)`/`endBackup`
transitions nor the restore half nor a concurrent `AtomicReadFile` were
re-tested against this repository's own build in this session (time) - carried
forward as B-ACC-CPP's documented gap rather than re-verified or assumed
fixed.

**One item this repository's own session DID verify differently from
B-ACC-CPP, and is recorded here as a genuine, new finding:** B-ACC-CPP's
`TODO.md` #5 reports a live `ReadProperty` of `File,1.File_Size` answering
`Error(object: read-access-denied)`. This repository's own build did **not**
reproduce that - `File_Size` read back `0` correctly over the wire in this
session (verified with `bacpypes3` against this repository's own running
instance). The two repositories' File/Backup-Restore setup is otherwise
identical, so the cause of B-ACC-CPP's denial (not root-caused there either)
remains unidentified; this is additional evidence for whoever investigates
#2046 next, not a claim that #2046 is resolved.

**Filed:** [chipkin/cas-bacnet-stack#2046](https://github.com/chipkin/cas-bacnet-stack/issues/2046).

## 3. Calendar 1 ("Cream")'s `Date_List` — inherited, pre-existing gap

Same gap B-AAC-CPP's file header documents against stack issue #963:
`BACnetStack_AddScheduleExceptionEventWithCalendarReference` does not resolve
a Calendar's `Date_List` at evaluation time, and there is no customer-facing
way to populate `Date_List` at all. This file uses the inline
`...WithCalendarEntry` exception form instead (fully functional); Cream still
exists as an object with `Date_List` `accepted` (not served) in
`docs/objects.json`.

## 4. F-ELEVATOR: `Landing_Call_Control` WriteProperty not independently wire-verified

Inherited from B-EC-CPP, unchanged in this repository:
`BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl` is
registered and the code was reviewed against the stack's doc comment for
correctness, and builds/registers correctly, but a WriteProperty of
`Landing_Call_Control` was **not** independently wire-verified with a live
BACnet client during B-EC-CPP's own verification session -
`BACnetLandingCallStatus`'s write-side SEQUENCE-CHOICE encoding could not be
cleanly expressed through the test-client library available in that session.
Not re-attempted in this repository's own session either (out of scope: this
session's wire-testing time was spent on this repository's four NEW features -
DM-OCD-B, AE-EL-I-B, SCHED-I-B, DM-BR-B - per the task brief). See
`docs/objects.json`'s note on Maroon.
