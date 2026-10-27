# TODO - known gaps (each checked against the pinned stack source and/or the wire, not assumed)

Stack pin: `69b6249f` (6.x, reports 6.0.23). Re-checked 2026-10-26
when the pin moved from `29b83de6` (items 2 and 4 still reproduce; item 3 is
fixed at this pin; Wire Tests).

This example is **Profile-functional** (series `docs/GLOSSARY.md`) since 1.2.0:
a landing call moves the car Simulation, and CreateObject/DeleteObject, the
Schedule over TimeSynchronization, the Event Log over ReadRange and
backup/restore are Wire Tests with their Failure Modes (`tests/README.md`).
Items 2-4 below are `[KNOWN GAP]` lines in a Wire Test: reported on every run,
not counted as a failure, outside this example's scope.

## Stack defects found at this pin

### 1. (withdrawn) Event Log `Total_Record_Count`

Not a stack defect. The probe read property 322, which is Access_Event_Tag. Total_Record_Count is
**145**, and 145 is in the `Property_List` captured above (`[..., 144, 145, 173]`), so the
property is present. A ReadProperty of 322 correctly answers unknown-property.

### 2. No customer way to start an Event Log enabled

Already filed: [#3301](https://github.com/chipkin/cas-bacnet-stack/issues/3301).
Wire Test: `tests/event_log_test.py`, `[KNOWN GAP] Event Log 1 is enabled at
start-up` (it reads FALSE on Windows and Linux at this pin).

`Enable` (cl. 12.27.8) defaults to FALSE and `BACnetStack_AddEventLogObject`
takes no enable argument; the customer header has no other call that sets it
(Trend Logs have `BACnetStack_SetTrendLogTypeTo*(..., enable, ...)`). So Beige
logs **nothing** until a client writes `Enable` TRUE. Checked on the wire: with
`Enable` FALSE, `Record_Count` stays 0 across a Passenger_Alarm transition;
after a WriteProperty of `Enable` TRUE (SimpleACK) it logs. L.13.1 asks for
"Logging of event notifications of the local device in an Event Log object";
today that needs an operator's write first. The README says so.

### 3. (fixed at `69b6249f`) The device's own broadcast notification is logged twice

Fixed upstream (stack PR #4011, in the 6.x tip this example is pinned to). At
`69b6249f` the Event Log Wire Test's "logged once" check passes: one record per
transition. The record below is kept for history.

Filed: [#3957](https://github.com/chipkin/cas-bacnet-stack/issues/3957). Verification found it is the stack's fault, not `common/`'s.
Wire Test: `tests/event_log_test.py`, `[KNOWN GAP] Each transition is logged
once` - 2 records of one transition, process identifiers 0 and 1, on Windows
and Linux (2026-10-16).

With `Enable` TRUE, one Passenger_Alarm transition gives `Record_Count` 3: the
log-status record for the Enable write, then the **same** notification twice -
same timestamp and content, process identifier 0 on one and 1 on the other.
The device sends one UnconfirmedEventNotification to Notification Class 1's
broadcast recipient (process identifier 1), the stack offers it to the Event
Log as initiated (`wasReceived = false`, before the per-recipient loop), and
then the device hears its own broadcast back from its own IP and the stack logs
it again as received. ReadRange of Log_Buffer, Windows build:

```
TX 59 bytes to 192.168.3.255:47888 (Network Port 1) - Unconfirmed: UnconfirmedEventNotification
RX 59 bytes from 192.168.3.64:47888 (Network Port 1) - Unconfirmed: UnconfirmedEventNotification
ReadRange Log_Buffer: item count 3
  record 2: ... 1e 09 00 1c 02 05 ef 96 ...   (process-identifier 0, initiating device 389014)
  record 3: ... 1e 09 01 1c 02 05 ef 96 ...   (process-identifier 1, same timestamp and body)
```

`BACnetDBDevice.h` (`OfferLocallyOriginatedNotificationToForwarders`) says the
design intends Event Log to see a locally originated notification once. Whether
the fix belongs in the stack (ignore a received notification from its own
address) or in `common/` (drop datagrams from its own address) is for the
coordinator to decide.

### 4. A refused landing call cannot name its error

Filed: [#3966](https://github.com/chipkin/cas-bacnet-stack/issues/3966).


**Stack defect.** `CallbackSetElevatorGroupLandingCallControl`
(`BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl`, `CASBACnetStackDLL.h`)
has no `errorCode` out-parameter, unlike every `CallbackSetProperty*`. When the
application refuses a call for a floor the installation does not have, the stack
falls through to its primitive-datatype switch (`BACnetBusinessLogic::SetValueUsingCallbacks`,
"Unsupported datatype for callbacks", then `SetStoredDBValue` "Object does not have a
value for property=[471]") and the client gets **PROPERTY / WRITE_ACCESS_DENIED**
(class 2, code 40). 135-2024 cl. 15.9.1.3.1 names PROPERTY / VALUE_OUT_OF_RANGE for
"The value provided is outside the range of values that the property can take on",
and WRITE_ACCESS_DENIED means the property "is currently not writable by the
requestor" - the wrong message to the management station. Wire Test:
`tests/elevator_test.py`, `[KNOWN GAP] The refusal of floor 11 / floor 0 is PROPERTY
/ VALUE_OUT_OF_RANGE`; the refusal itself (an Error, and the car not moving) is a
checked requirement. Seen on Windows and Linux, 2026-10-16:

```
WriteProperty: Elevator Group 1 (Maroon) Landing_Call_Control REFUSED - floor 11 is not served (floors 1..10)
::CASBACnetStack::BACnetBusinessLogic::SetValueUsingCallbacks() ... Error: Unsupported datatype for callbacks
::CASBACnetStack::BACnetDBDevice::SetStoredDBValue() ... Error: Object does not have a value for property=[471] in the stack memory
[KNOWN GAP] The refusal of floor 11 is PROPERTY / VALUE_OUT_OF_RANGE (cl. 15.9.1.3.1) - ... (got Error class=2 code=40)
```

## Closed upstream - re-checked at this pin and removed from this file

- **#2045** (AddEventLogObject "Failed to set the date/time" log flood) - CLOSED
  2026-09-16. Not reproduced at `64b2cfac5`: 0 such lines in 10 s of running.
- **#2046** (tracking, still OPEN) - its children: #2151 and #2152 (File_Size
  read-access-denied, AtomicReadFile abort during backup) CLOSED 2026-09-18;
  #2150 (access-family constructed properties) is open but does not touch this
  example. The DM-BR-B gap this file carried is now checked on the wire - see
  "Checked by hand" below.
- **#963** (Calendar `Date_List` / calendar-reference exception) - CLOSED
  2026-09-13. The customer header now has `BACnetStack_AddCalendarObject`,
  `AddCalendarDateEntry`, `AddCalendarDateRangeEntry`, `AddCalendarWeekNDayEntry`
  and `AddScheduleExceptionEventWithCalendarReference`. Calendar 1 (Cream) is
  still the old `AddObject` + an app `Present_Value` that always answers FALSE,
  and Saffron's exception still uses the inline calendar-entry form. Moving Cream
  to a stack-held Calendar that drives Saffron (as B-BC-CPP 1.0.7 did) is this
  example's own work, no longer blocked.

## Now Wire Tests (were checked by hand at 1.1.0)

- **DM-BR-B, backup and restore**: `tests/backup_restore_test.py` - a full
  backup, a changed configuration, a restore that brings it back, a wrong
  password and an invalid image.
- **AE-EL-I-B**: `tests/event_log_test.py` - Enable, a logged Passenger_Alarm
  transition, ReadRange (whole buffer and by position), Enable FALSE again.
- **Landing_Call_Control**: `tests/elevator_test.py` - the write now calls the
  car Simulation, which answers it.

## Not done

- Calendar 1 (Cream) is still the old `AddObject` Calendar whose
  `Present_Value` always answers FALSE; Saffron's one exception uses the inline
  calendar-entry form. Moving Cream to `BACnetStack_AddCalendarObject` (as
  B-BC-CPP did) is open, not blocked.
- By default the Lift's EventNotifications go to Notification Class 1's one
  recipient, the local subnet **broadcast**, which a Python Peer bound to
  127.0.0.1 cannot hear on Windows. The Recipient_List is writable, so
  `tests/recipient_list_test.py` writes the Peer into it and receives the
  alarm and its return to normal on the wire.
- The Lift's simulated `Passenger_Alarm` flips on a timer (every 30 s, or the
  `BAEC_TEST_PASSENGER_ALARM_SECONDS` test variable) and cannot be switched off from
  BACnet or the console, so a test that needs the car quiet has to wait for it.
  (ref: chipkin/chipkin-bacnet-testing-tool#615)
- Notification Class 1's `Recipient_List` is writable (cl. 12.21) but is not kept across a
  restart: the seeded broadcast recipient returns.
