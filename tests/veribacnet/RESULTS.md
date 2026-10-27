# VeriBACnet results

The latest VeriBACnet run of this folder's project, summarised by hand from
that run's `report.json`. The run's own Results Folder (reports, packet
captures, wire logs) is not published: it holds the testing machine's network
addresses.

| | |
|---|---|
| Date | 2026-10-27 |
| Example | BACnet B-AEC (Advanced Elevator Controller) Example - C++ v1.2.0 (`common/` 3.5.0), CAS BACnet Stack `6.x` @ `69b6249f` |
| Profile | B-AEC (BACnet Advanced Elevator Controller) |
| Transport | BACnet/IP, UDP port 47834 |
| VeriBACnet | 0.20.0 (its test device uses CAS BACnet Stack 6.0.23.0) |
| Password | The example was started with a DeviceCommunicationControl / ReinitializeDevice password (`--dcc-password`, a test value), and the project gives VeriBACnet the same password |
| Test plan | 284 BTL tests (ANSI/ASHRAE 135.1-2023) selected for the B-AEC profile and the options this device claims |
| Result | **123 PASS · 2 FAIL · 0 ERROR · 159 SKIP** |

**Judged BTL-clean on BACnet/IP, with a star**: no failure or error is the
example's own fault, every other failure is tracked, and every skipped test has
a stated reason. The star means some of the functions the example exists for
were not measured by VeriBACnet in this run (below). See "What BTL-clean means"
in [README.md](README.md). This is not BTL certification.

**Measured coverage:** 125 of the 284 selected tests ran (123 passed, 2
failed). Of the 159 skipped, 82 wait on a test-tool or test-script fix
(tracked), 44 need an operator, 31 do not apply to this device, and 2 wait on
this example (its simulated alarm cannot be switched off, and a written
Notification Class recipient list lasts only until the example restarts, so
the test that restarts the device with a written list was not set up). The 31
that do not apply: it offers COV on Analog Input 1 only (the Analog Output
refuses a COV subscription, so the tests that pick it skip), its Event_Enable
and Calendar date list are not writable, its one
event-reporting object (the Lift) does not report fault transitions, it has no
Event Enrollment object, no writable array, time or range-limited REAL
property and no writable non-commandable Present_Value, no object's name or
identifier can be changed over BACnet, it refuses the deprecated
DeviceCommunicationControl DISABLE option as its Protocol_Revision requires,
it supports every ReinitializeDevice state, and six Schedule tests apply only
to devices of an older Protocol_Revision.

**Measured now:** ReadProperty and ReadPropertyMultiple (all 24 tests passed),
WriteProperty and WritePropertyMultiple (all 24 that ran passed), COV notifications (DS-COV-B: 9 tests passed,
including a subscription and its notifications; the threshold test now
subscribes to Analog Input 1, reads its COV_Increment and receives the first
notification, then stops for an operator to change the input), COV-multiple notifications
(DS-COVM-B: all 9 that ran passed), the Event Log read over ReadRange
(AE-EL-I-B: 4 tests passed), Who-Is and Who-Has (24 of 26 passed; the other 2
need an operator), object creation and deletion (DM-OCD-B: 11 of the 13 that ran passed;
the 2 failures are test-script defects), the backup and restore session rules
and passwords (DM-BR-B: 8 tests passed), the full backup and restore up to the
step that needs an operator (below), the DeviceCommunicationControl password
(a wrong or missing password is refused), command prioritization on the
Analog Output, TimeSynchronization, the Read Support and EPICS consistency
tests, and the Multi-state Input's State_Text.

**Not measured yet:** event notification (AE-N-I-B), alarm acknowledgement
(AE-ACK-B), GetEventInformation (AE-INFO-B), scheduling (SCHED-I-B), the Who-Is
this device sends (DM-DDB-A), DeviceCommunicationControl disable-initiation
(DM-DCC-B), the warm and cold restarts of ReinitializeDevice (DM-RD-B) and the
full backup and restore test as a whole (DM-BR-B). The event-notification and
alarm-acknowledgement tests stop for an operator (to raise or clear the lift's
alarm), and the rest of the acknowledgement tests cannot yet be given the
values they compare; the "no active events" GetEventInformation test needs a
device with no active alarm, and this example's simulated alarm cannot be
switched off; the Schedule and Calendar tests cannot yet be given the scenario
data they need; the Who-Is is sent when a person presses `d`; the
disable-initiation tests need a COV change the test tool can cause, and this
device's COV point is an input; the restart tests stop for an operator to
confirm the restart; the full backup and restore test passed every step except
the one where an operator changes the configuration between the backup and
the restore (the test tool does not yet make that change itself, although
this device accepts CreateObject). All of it is tracked. This example's own Wire Tests cover these
functions: landing calls written and served by the lift car, the car followed
over ReadPropertyMultiple, objects created, written and deleted, the Schedule
run over TimeSynchronization, the Event Log read over ReadRange, the lift's alarm
delivered through a written recipient list, and a backup
and restore.

This run replaces the earlier 2026-10-26 run (284 tests, 115 PASS, 9 FAIL,
124 measured). Nothing in the example changed; the test tool did. Its scripts
now take the objects and values they write from the device's own description
instead of a fixed writable Analog Value, so the WriteProperty,
WritePropertyMultiple and list-write tests that used to fail before sending
anything now pass (9.22.1.5, 9.23.1.3, 9.23.1.8, 9.23.2.3, 9.23.2.15) or
run (9.22.2.1, 7.2.2); the Subscribe-COV-Property-Multiple test 9.42.1.2
passes; and the two COV threshold tests (8.2.1, 8.3.1) now skip for an
operator instead of failing.

The run before that was the 2026-10-24 run (253 tests, 118 measured). Since then:
the example moved to CAS BACnet Stack `6.x` @ `69b6249f`; File 1's File_Size
became writable, so a restore can empty the file before writing the backed-up
image (the full backup and restore test now passes that step); the project
declares which Test Plan options the device claims (writable datatypes, COV on
Analog Input objects, the CHANGE_OF_STATE alarm, REAL schedule values,
password-protected backup and restore, Analog Value creation), which selects
27 more tests; and the test tool now runs the ReadRange tests against the
Event Log (they pass), the WritePropertyMultiple error tests (they pass) and
the Active_COV_Subscriptions test (it passes). Tests that used to fail before
sending anything because they wanted an Analog Value now either pass or skip
with a reason.

The profile lets a device claim DM-TS-B or DM-UTC-B; this example claims
DM-TS-B only, and that is what was tested.

## Tests that did not pass

| Test | Result | Why |
|---|---|---|
| 9.16.2.1 CreateObject with an identifier that already exists | FAIL | The script names Analog Value 1 as the "existing" object; this device has no Analog Value until a client creates one, so it correctly created it. A test-script defect, tracked. |
| 9.16.2.4 CreateObject with an error in the initial values | FAIL | The script's first initial value names the new object; this device does not let a client name a created object and correctly refuses that first value, so the faulty value the test is about is never reached. A test-script defect, tracked. |

## Skipped tests, by reason

| Tests | Why |
|---|---|
| 9.1.1.1 to 9.1.1.6 and 9.1.1.14 (AcknowledgeAlarm) | The tests reach the device and then need an operator to put the lift's alarm into its normal state; the run was unattended. |
| 9.1.1.8 to 9.1.1.11, 9.1.1.15, 9.1.2.x (6) and 7.3.1.11.1 (AcknowledgeAlarm) | The tests compare against configured values the test tool does not derive from the device's description. Tracked. |
| 9.42.x SubscribeCOVPropertyMultiple (12) | The test tool cannot yet derive the scenario data these tests need (several subscriptions, lifetimes, increments, capacity, notification values). Tracked. |
| 7.3.2.23.x Schedule (25), 7.2.4 / 7.2.10 / 7.2.11 date-pattern properties (3), 7.3.2.24.x Event Log (8) | The test tool cannot yet be given the expected-value scenarios these tests need for a device it did not build. Tracked. |
| 9.21.1.4.1, 9.21.1.7 to 9.21.1.10, 9.21.2.4 (ReadRange) | The tests compare against the log's own timestamps or sequence numbers, which are not declared and have no live form yet. Tracked. |
| 13.8.1.1 (full backup and restore) | Every step passed (the backup, the restore of File 1 including its File_Size, and the object list and names afterwards) except the one where an operator changes the configuration between the backup and the restore; the run was unattended. The test tool could make that change itself through CreateObject, which this device accepts. Tracked. |
| 8.2.1, 8.2.2, 8.3.1, 8.3.2 (COV threshold and status-flag tests) | The tests subscribe to Analog Input 1 and receive the first notification, then need a person to change the input's value or status flags (the device correctly refuses a write to it); the run was unattended. |
| 9.23.1.2 | The test configuration does not yet name an object with two writable properties. Tracked. |
| 9.16.2.5 | The script skips on a fixed rule about the test tool's own stack. Tracked. |
| 9.23.2.22 | The test tool cannot yet encode the date-range value this test writes. Tracked. |
| 9.8.2, 9.8.3 (automated companions) | They raise an alarm on a test object this device does not have; the official 9.8.x tests cover the device's own events. Tracked. |
| 9.8.1 Event Information with no Active Events | The test needs a device with no active alarm. This example simulates the lift's passenger alarm on a 30-second timer that cannot be switched off, so the test did not make its check. |
| 15 tests (7.2.8, 7.3.1.19.x, 7.3.1.20.x, 7.3.1.22.x, 8.4.17.10, 8.5.17.10, 7.3.2.35.x, 7.3.2.36.x) | Need an operator; the run was unattended. |
| 8.34.2 Who-Is initiation | Needs an operator: the device sends its Who-Is when a person presses `d`. |
| 9.27.1.2, 9.27.1.4, 9.27.2.3, 9.27.2.4 (ReinitializeDevice cold and warm start, with and without the password) | Need an operator to confirm the restart; the run was unattended. |
| 7.3.2.10.9 / 7.3.2.10.10 Device Object_Name / Object_Identifier configurable, 9.32.1.12 / 9.32.1.13 Who-Has after a change | Need an operator: the name and instance are configured by restarting the device with `--deviceName` / `--deviceID`. |
| 9.10.1.4, .5, .7, .9, .11 COV tests, 9.24.1.8 to 9.24.1.10 disable-initiation tests | The test tool chose the Analog Output, which correctly refuses a COV subscription on this device; its COV point, Analog Input 1, cannot be changed over BACnet. |
| 8.4.2 / 8.5.2 CHANGE_OF_STATE tests (10, with their automated companions) | Need an object whose Event_Enable includes fault transitions; the Lift's alarm has no fault source, so it reports to-offnormal and to-normal only. |
| 7.3.2.10.5 / .6 Database_Revision after a rename | No object's name or identifier can be changed over BACnet; the clause skips the tests then. |
| 7.3.1.10.1, Calendar tests 7.3.2.8.1 to .3 | Need a writable Event_Enable or Calendar date list; this device's are fixed in `main.cpp`. |
| Notification Class recipient tests 7.3.2.21.3.1, .2, .4, .5, .6 | The test tool accepts these scenarios only with an Event Enrollment object as the event source; this device reports through the Lift's own alarm. Tracked. |
| 7.3.2.21.3.7 Recipient_List kept through a restart | Not set up: a written recipient list lasts until the example restarts (the example keeps no configuration on disk), so the test's power-off restart would lose it. Waits on this example. |
| 9.8.2, 9.8.3, 9.8.4 (Event Enrollment) | Need an Event Enrollment object; this device has none. |
| 9.8.6 GetEventInformation chaining | The device cannot hold enough active events to need a second response, which the clause allows. |
| 7.3.1.12 Notify_Type Test | Needs one event-reporting analog object that reports alarms and another that reports events; this device's only event-reporting object is the Lift. |
| 9.22.1.6, 9.22.2.4, 9.23.2.7, 9.22.2.10, 9.23.2.13, 9.22.2.11, 9.23.2.18, 9.23.2.20 | Need a writable non-commandable Present_Value, a range-limited REAL, a writable fixed-size array, an optional-functionality write or a writable time property; this device has none. |
| 9.24.1.3, 9.24.1.4 | Test the DeviceCommunicationControl DISABLE option, which this Protocol_Revision deprecates; the device correctly refuses it (9.24.1.11 passed). |
| 7.3.2.23.3.2 to 7.3.2.23.3.7 | Apply only to devices of Protocol_Revision 3 or lower. |
| 9.27.2.5 ReinitializeDevice unsupported states | The device supports every ReinitializeDevice state, so there is no unsupported state to try. |

Five more B-AEC requirements have no runnable VeriBACnet test yet, so they
were not run: 7.3.1.11.3 and 7.3.1.11.4 Acked_Transitions tests, 7.3.2.23.15
Write_Every_Scheduled_Action FALSE Test, and the 8.4 and 8.5 event
notification initiation tests. All tracked.

No public issue has been opened for any of these.
