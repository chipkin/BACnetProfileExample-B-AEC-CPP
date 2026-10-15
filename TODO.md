# TODO - known gaps (each checked against the pinned stack source and/or the wire, not assumed)

Stack pin: `64b2cfac5` (6.x, 2026-10-15, reports 6.0.23). Re-checked 2026-10-15
when the pin moved from `abd4cee1` (6.0.21).

This example is **BIBB-conformant, not yet Profile-functional** (series
`docs/GLOSSARY.md`): the elevator does not move, and only discovery is a Wire
Test. That work is the example's own scope, not blocked on the stack.

## Stack defects found at this pin (not filed - for the series coordinator to verify and file)

### 1. Event Log `Total_Record_Count` is missing

135-2024 cl. 12.27, the Event Log property table, lists `Total_Record_Count`
(Unsigned32) as **R**. At `64b2cfac5`, Event Log 1 (Beige)'s `Property_List`
does not contain it and a ReadProperty answers Error `property` /
`unknown-property` (class 2, code 32), on Windows (STATIC) and Linux (SOURCE):

```
Property_List: [36, 111, 126, 131, 133, 137, 140, 141, 144, 145, 173]
ReadProperty Event Log 1 Total_Record_Count (322): Error class=2 code=32
BACnetBusinessLogic.cpp(1728) - Error: Property does not exist, objectType[25], objectInstance=[1], propertyIdentifier=[322]
```

The example does not disable it, and `BACnetStack_SetPropertyEnabled(..., 322,
true)` is refused, so there is no workaround here. The stack's own
`BACnetDBPropertyProfile::GetRequiredPropertiesForEventLog` lists
`totalRecordCount`, and `CASBACnetStackDLL.h`'s `BACnetStack_AddEventLogObject`
doc says the stack stores and serves it. The discovery Wire Test cannot see
this: it reads only what `Property_List` lists.

### 2. No customer way to start an Event Log enabled

`Enable` (cl. 12.27.8) defaults to FALSE and `BACnetStack_AddEventLogObject`
takes no enable argument; the customer header has no other call that sets it
(Trend Logs have `BACnetStack_SetTrendLogTypeTo*(..., enable, ...)`). So Beige
logs **nothing** until a client writes `Enable` TRUE. Checked on the wire: with
`Enable` FALSE, `Record_Count` stays 0 across a Passenger_Alarm transition;
after a WriteProperty of `Enable` TRUE (SimpleACK) it logs. L.13.1 asks for
"Logging of event notifications of the local device in an Event Log object";
today that needs an operator's write first. The README says so.

### 3. The device's own broadcast notification is logged twice

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

## Checked by hand at this pin (not yet Wire Tests)

On 2026-10-15, Windows and Linux, after the 1.1.0 fixes:

- **DM-BR-B, backup and restore**: ReinitializeDevice STARTBACKUP (SimpleACK,
  `System_Status` backup-in-progress), AtomicReadFile of File 1, ENDBACKUP,
  STARTRESTORE, AtomicWriteFile of 15 octets, ENDRESTORE - every step
  acknowledged, the four backup/restore callbacks ran, and File 1 then read
  back the restored content. Before 1.1.0 the example's ReinitializeDevice
  callback refused every backup/restore state, so no backup could start.
- **AE-EL-I-B**: with `Enable` written TRUE, a Passenger_Alarm transition is
  logged and ReadRange returns the records (stack defects 2 and 3 above apply).
  Before 1.1.0 ReadRange was not enabled, so Log_Buffer could not be read.
- **Landing_Call_Control**: before any write it reads floor 0, direction
  `none`; a raw-encoded WriteProperty (`09 07 19 03`, floor 7 up) is
  acknowledged and reads back unchanged. Nothing moves the car.

## The example's own next steps (phase 2)

- A car simulation that answers a landing call (position, direction, doors),
  with a Wire Test that fails if the handler is removed.
- Wire Tests for CreateObject/DeleteObject, the Schedule driving Chartreuse,
  the Event Log capturing an alarm, and the backup/restore cycle above.
