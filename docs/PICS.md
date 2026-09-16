# BACnet Protocol Implementation Conformance Statement (PICS)

For the **BACnet B-AEC (Advanced Elevator Controller) C++ example** -
see [README.md](../README.md).

> This is the PICS **for the example as shipped**. It describes a tutorial
> device announcing itself as a Chipkin demo, not a product. When you turn this
> example into your own device, this document is one of the things you rewrite:
> the vendor, model and version rows all come from the
> `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`. The
> example has **not** been submitted for BTL certification.

## 1. Product description

| | |
|---|---|
| **Vendor Name** | Chipkin Automation Systems |
| **Vendor Identifier** | 389 |
| **Product Name** | CAS BACnet Stack Example - B-AEC |
| **Product Model Number** | CAS BACnet Stack Example - B-AEC |
| **Application Software Version** | 1.0.0 |
| **Firmware Revision** | 1.0.0 |
| **BACnet Protocol Version** | 1 |
| **BACnet Protocol Revision** | 24 |

**Product Description:** a BACnet/IP elevator-installation controller built on
the CAS BACnet Stack. It reports what an elevator installation is doing
(car position, doors, alarms) and accepts commands (three commandable outputs,
the Elevator Group's landing calls), generates intrinsic alarm notifications,
captures an Event Log, drives an output from a Schedule, can be backed up and
restored, and lets a management station create and delete an Analog Value
object at runtime (CreateObject/DeleteObject). It is a tutorial for
implementers of the B-AEC profile, and this repository's canonical example for
DM-OCD-B / F-OCD.

## 2. BACnet standardized device profile (Annex L)

**B-AEC - BACnet Advanced Elevator Controller.**

This device claims exactly one profile. Because the B-AEC requirements are a
superset of B-EC's, which are in turn a superset of B-EM's and B-GENERAL's, a
conformant B-AEC device also satisfies **B-EC** and **B-EM** (Annex L.14 /
L.13) and **B-GENERAL** (Annex L.8); that is subsumption, not additional
claims.

## 3. BIBBs supported (Annex K)

| BIBB | Description |
|---|---|
| DS-RP-B | Data Sharing - ReadProperty - B |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B |
| DS-WP-B | Data Sharing - WriteProperty - B |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B |
| DS-COV-B | Data Sharing - COV - B |
| DS-COVM-B | Data Sharing - COV Multiple - B |
| AE-N-I-B | Alarm and Event - Notification Internal - B |
| AE-ACK-B | Alarm and Event - ACK - B |
| AE-INFO-B | Alarm and Event - Information - B |
| AE-EL-I-B | Alarm and Event - Enrollment Log - Internal - B |
| SCHED-I-B | Scheduling - Internal - B |
| DM-DDB-A | Device Management - Dynamic Device Binding - A |
| DM-DDB-B | Device Management - Dynamic Device Binding - B |
| DM-DOB-B | Device Management - Dynamic Object Binding - B |
| DM-DCC-B | Device Management - Device Communication Control - B |
| DM-TS-B | Device Management - TimeSynchronization - B |
| DM-OCD-B | Device Management - Object Creation and Deletion - B |
| DM-RD-B | Device Management - ReinitializeDevice - B |
| DM-BR-B | Device Management - Backup and Restore - B |

No other BIBBs are supported. In particular this device does **not** support
DM-UTC-B (it uses local-time TimeSynchronization, not UTC), any Access-Control
(DS-AC*/AE-AC*) BIBB, or Trending (T-*).

## 4. Application services supported

| Service | Initiate | Execute |
|---|:---:|:---:|
| ReadProperty | no | **yes** |
| ReadPropertyMultiple | no | **yes** |
| WriteProperty | no | **yes** |
| WritePropertyMultiple | no | **yes** |
| SubscribeCOV | no | **yes** |
| SubscribeCOVPropertyMultiple | no | **yes** |
| ConfirmedEventNotification | **yes** | no |
| UnconfirmedEventNotification | **yes** | no |
| AcknowledgeAlarm | no | **yes** |
| GetEventInformation | no | **yes** |
| CreateObject | no | **yes** |
| DeleteObject | no | **yes** |
| AtomicReadFile | no | **yes** |
| AtomicWriteFile | no | **yes** |
| DeviceCommunicationControl | no | **yes** |
| ReinitializeDevice | no | **yes** |
| TimeSynchronization | no | **yes** |
| Who-Is | **yes** (on demand, the `d` key) | **yes** |
| I-Am | **yes** | - |
| Who-Has | no | **yes** |
| I-Have | **yes** | - |

Intrinsic alarming on Lift 1 ("Mauve")'s `Passenger_Alarm` generates an
EventNotification to the recipients of Notification Class 1 ("Crimson") on
each ChangeOfState transition. An unsolicited I-Am is broadcast to the local
subnet at start-up, as well as in response to Who-Is.

## 5. Segmentation capability

Segmentation is **not supported** in either direction
(`Segmentation_Supported` = `no-segmentation`). `Max_APDU_Length_Accepted` is
1476 octets, the BACnet/IP maximum.

## 6. Standard object types supported

Analog Value is the **only** object type marked creatable
(`BACnetStack_SetObjectTypeCreatable`); a client can CreateObject/DeleteObject
instances of it at runtime (DM-OCD-B) - no Analog Value instance exists at
start-up. No other object type is dynamically creatable or deletable. The
three F-OUTPUTS objects' `Present_Value`, the Elevator Group's
`Landing_Call_Control`, the Event Log's `Enable`, and the File's `Archive` are
writable; every other property of every object is read-only.

| Object type | Instance | Object_Name | Optional properties supported |
|---|:---:|---|---|
| Device | 389014 | Rainbow | Description |
| Analog Input | 1 | Bronze | - |
| Binary Input | 1 | Emerald | - |
| Multi-State Input | 1 | Hot Pink | State_Text |
| Analog Output | 1 | Chartreuse | Priority_Array, Relinquish_Default (commandable) |
| Binary Output | 1 | Fuchsia | Priority_Array, Relinquish_Default (commandable) |
| Multi-State Output | 1 | Indigo | Priority_Array, Relinquish_Default (commandable) |
| Network Port | 1 | Vermilion | - |
| Elevator Group | 1 | Maroon | Group_Mode, LandingCalls; `Landing_Call_Control` writable |
| Lift | 1 | Mauve | Assigned_Landing_Calls, Registered_Car_Call, Landing_Door_Status |
| Escalator | 1 | Mint | Power_Mode, Escalator_Mode |
| Positive Integer Value | 1 | Turquoise | - |
| Notification Class | 1 | Crimson | - |
| Event Log | 1 | Beige | `Enable` writable |
| Schedule | 1 | Saffron | - |
| Calendar | 1 | Cream | - |
| File | 1 | Ivory | `Archive` writable |
| Analog Value | *(dynamic)* | *(created via DM-OCD-B)* | `Present_Value` writable; the only creatable/deletable object type |

The device instance is configurable at run time with `--deviceID` (BACnet
requires the device instance to be configurable).

## 7. Data link layer options

**BACnet/IP (Annex J)**, UDP port 47808 (0xBAC0) by default, configurable at run
time with `--port`.

BBMD is not supported, Foreign Device registration is not supported, and
BACnet/SC, MS/TP, Ethernet (Annex H) and PTP are not supported.

## 8. Device address binding

Static device binding is **not supported**. The device answers Who-Is/Who-Has
and does not initiate any confirmed request that requires binding a peer; the
`d` key's demo Who-Is (DM-DDB-A) is a broadcast, not a binding lookup.

## 9. Networking options

None. The device is not a router, not a BBMD, and does not register as a
foreign device.

## 10. Character sets supported

UTF-8 (ANSI X3.4). Supporting a character set does not imply the device can
handle data in all character sets.

## 11. Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the stack's `docs/property-profile-reference.md`), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Device 389014 "Rainbow" - the device itself; the instance is configurable with --deviceID. The stack rows are device-wide facts only the stack knows - the protocol version and revision it implements, the services and object types it was configured with (17 base objects plus the dynamically creatable Analog Value), the live object list and address-binding table. The accepted rows are the stack's configured defaults for APDU limits, segmentation, system status and database revision; an application that answered them from its own constants could contradict the stack, so this example does not

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| System_Status | BACnetDeviceStatus | stack default, accepted (Generic Enumerated default: `0`) | no |
| Vendor_Name | CharacterString | app | no |
| Vendor_Identifier | Unsigned16 | app | no |
| Model_Name | CharacterString | app | no |
| Firmware_Revision | CharacterString | app | no |
| Application_Software_Version | CharacterString | app | no |
| Description *(optional, enabled)* | CharacterString | app | no |
| Protocol_Version | Unsigned | stack | no |
| Protocol_Revision | Unsigned | stack | no |
| Protocol_Services_Supported | BACnetServicesSupported | stack | no |
| Protocol_Object_Types_Supported | BACnetObjectTypesSupported | stack | no |
| Object_List | BACnetARRAY[N] of BACnetObjectIdentifier | stack | no |
| Max_APDU_Length_Accepted | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_MAX_APDU_LENGTH_ACCEPTED`) | no |
| Segmentation_Supported | BACnetSegmentation | stack default, accepted (`BACnetSegmentation::noSegmentation`) | no |
| APDU_Timeout | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_APDU_TIMEOUT`) | no |
| Number_Of_APDU_Retries | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_NUMBER_OF_APDU_RETRIES`) | no |
| Device_Address_Binding | BACnetLIST of BACnetAddressBinding | stack | no |
| Database_Revision | Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Event Log 1 "Beige" - AE-EL-I-B (F-EVENTLOG, NEW vs B-EC; this series' canonical pattern is B-ALSC-CPP). Enable/Buffer_Size/Record_Count/Total_Record_Count/Log_Buffer/Status_Flags/Object_Identifier/Object_Type/Property_List are NOT stack defaults in the generic sense - they are genuinely held and served by the stack's own Event Log engine (BACnetStack_AddEventLogObject), answered from stack storage BEFORE this file's Get*/Set* callbacks are ever consulted; marked accepted only because property-profile-reference.md's generic table does not know about this object-specific engine. VERIFIED over the wire with a live bacpypes3 client against THIS repository's own build: Object_Name (undefined, the stack's generic non-Device default - no callback serves it), Record_Count and Total_Record_Count (both 0) all read correctly. VERIFIED DEFECT, independently reproduced in this repository (chipkin/cas-bacnet-stack#2045, first found by B-ACC-CPP): adding this object alone causes a continuous, non-fatal 'Failed to set the date'/'Failed to set the time' internal log flood starting from the very first BACnetStack_Tick() - 200 occurrences of each line observed in a 5-second smoke test window. NOTE: B-ALSC-CPP's own task (same object, same BACnetStack_AddEventLogObject call, same stack pin) did NOT observe this flood and its TODO.md/objects.json make no mention of it - the two repositories' object combinations differ (this one also adds Schedule/Calendar/File/Backup-Restore/DM-OCD-B), so the trigger is not 'AddEventLogObject alone' as B-ACC's bisection concluded; it reproduces in at least one combination the same as B-ACC's (Event Log + Schedule/Calendar + File/Backup-Restore) and did not reproduce in B-ALSC's narrower one (Event Log + Schedule/Calendar, no File/Backup-Restore). Flagged in TODO.md with this comparison rather than re-asserting B-ACC's isolation claim unverified. The device otherwise keeps answering Who-Is/ReadProperty correctly throughout the flood (wire-verified)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Calendar 1 "Cream" - Exists for SCHED-I-B completeness alongside Saffron's exception, but its Date_List cannot be populated through the customer API (cas-bacnet-stack issue #963 - no read path for a Calendar object's Date_List; the only generic constructed-property callback is test-tool-only, same gap B-AAC/B-ALSC/B-ACC all document). Present_Value therefore always answers false rather than evaluating a Date_List that is never populated. VERIFIED over the wire: Present_Value read back as false (0)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Boolean | app | no |
| Date_List | BACnetLIST of BACnetCalendarEntry | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

<!-- OBJECTS-PROPERTIES:END -->

## 12. References

- ANSI/ASHRAE Standard 135-2024, Annex A (PICS template), Annex K (BIBBs),
  Annex L (device profiles), Clause 12 (object types), Clause 13 (alarm and
  event services).
- [README.md](../README.md) - what this example is and how to build it.
- [TUTORIAL.md](../TUTORIAL.md) - how to extend it, and how to keep this
  document honest when you do.
