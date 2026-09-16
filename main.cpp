// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE. The CAS BACnet Stack itself is
// a separate, commercially licensed product and is not covered by CC0.
// =============================================================================
// BACnet Profile Example - B-EC (Elevator Controller) - C++
//
// This example implements the BACnet "B-EC" (Elevator Controller) device
// profile: everything the B-EM (Elevator Monitor) example demonstrates, PLUS
// commands. Where a B-EM only ever reports what an elevator installation is
// doing, a B-EC is the controller a management station actually directs: it
// accepts WriteProperty/WritePropertyMultiple to a set of commandable outputs,
// a landing call on the Elevator Group, TimeSynchronization, and
// ReinitializeDevice.
//
// A B-EC (ANSI/ASHRAE 135, Annex L.14) must support:
//
//     DS-RP-B, DS-RPM-B       - ReadProperty + ReadPropertyMultiple,
//     DS-WP-B, DS-WPM-B       - WriteProperty + WritePropertyMultiple (NEW vs B-EM),
//     DS-COV-B                - SubscribeCOV (plain, whole-object),
//     DS-COVM-B               - SubscribeCOVPropertyMultiple,
//     AE-N-I-B                - generate intrinsic alarm/event notifications,
//     AE-ACK-B                - accept AcknowledgeAlarm,
//     AE-INFO-B               - answer GetEventInformation,
//     DM-DDB-A                - initiate discovery (NEW vs B-EM: send Who-Is,
//                               not just answer it),
//     DM-DDB-B, DM-DOB-B      - Who-Is/I-Am, Who-Has/I-Have (answer),
//     DM-DCC-B                - DeviceCommunicationControl,
//     DM-TS-B (or DM-UTC-B)   - TimeSynchronization (NEW vs B-EM),
//     DM-RD-B                 - ReinitializeDevice (NEW vs B-EM).
//
// WHAT IS NOT IMPLEMENTED (see README.md "What this example does NOT do" +
// TODO.md): nothing required by B-EC is missing at this stack pin.
//
// This example seeds from B-EM (Wave 2) and grafts in three feature blocks that
// are canonical elsewhere in the series, unchanged:
//
//     F-OUTPUTS   - commandable Analog/Binary/Multi-State Output objects (the
//                   Commandable struct, SetPropertyWritable(Present_Value), the
//                   Set/Null callbacks) - canonical in B-SA-CPP, copied here
//                   verbatim (constants, struct, helpers, callbacks).
//     F-TIMESYNC  - TimeSynchronization + Local_Date/Local_Time - canonical in
//                   B-LD-CPP, copied here verbatim (SyncedDateTime,
//                   InitSyncedDateTimeFromHost, SetSystemTime, GetPropertyDate/Time).
//     F-REINIT    - ReinitializeDevice with a deferred (post-ACK) restart -
//                   canonical pattern shared by B-AAC/B-ASC/B-LSC, copied here
//                   verbatim (PasswordAccepted, ReinitializeDevice, the
//                   CASExampleHelper::RequestRestart/RestartDue main-loop block).
//
// F-COVM and F-ELEVATOR (the Elevator Group / Lift / Escalator object family)
// are UNCHANGED from B-EM - see that repo's main.cpp for their original
// commentary, reproduced below wherever the code itself is unchanged.
//
// THE ONE GENUINELY NEW ELEVATOR PIECE: Elevator Group 1 ("Maroon")'s
// Landing_Call_Control property, which B-EM deliberately left unwritable (see
// its file header). This example registers
// BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl, so a
// WriteProperty naming a floor/direction/door now actually commands a landing
// call - the "controller" half of "Elevator Controller". The call is recorded
// and read back through the SAME GetListElevatorGroupLandingCallStatus callback
// B-EM registered, now also serving LandingCallControl (distinguished from
// LandingCalls by the propertyIdentifier parameter, which B-EM's read-only
// version ignored because it only ever answered LandingCalls).
//
// The device keeps the three base sensors, the elevator object family, and the
// alarming Notification Class from B-EM, and ADDS three commandable outputs.
// Each object has a colour name (the convention shared across this example
// series):
//
//     Device 389014              "Rainbow"      (instance configurable with --deviceID)
//     Analog Input  1            "Bronze"       (REAL, degrees Celsius; read-only)
//     Binary Input  1            "Emerald"      (active / inactive; read-only)
//     Multi-State Input 1        "Hot Pink"     (state 1..3; read-only)
//     Analog Output 1            "Chartreuse"   (REAL setpoint; commandable - NEW)
//     Binary Output 1            "Fuchsia"      (active/inactive; commandable - NEW)
//     Multi-State Output 1       "Indigo"       (state 1..3; commandable - NEW)
//     Network Port 1             "Vermilion"    (the BACnet/IP port - required)
//     Elevator Group 1           "Maroon"       (groups Mauve; a group of Lifts;
//                                                Landing_Call_Control now writable)
//     Lift 1                     "Mauve"        (car position, doors, alarm; intrinsic alarming)
//     Escalator 1                "Mint"         (not part of the Elevator Group - escalators
//                                                aren't lifts, so it uses the "no reference" sentinel)
//     Positive Integer Value 1   "Turquoise"    (the Elevator Group's Machine_Room_ID target -
//                                                must be added BEFORE the Elevator Group, below)
//     Notification Class 1       "Crimson"      (routes Mauve's Passenger_Alarm events)
//
// ALARMING: unchanged from B-EM. Lift 1 ("Mauve") arms an intrinsic ChangeOfState
// (boolean) algorithm on its Passenger_Alarm property, simulated on a 30-second
// timer (Passenger_Alarm is not writable through any API - it reflects a
// physical alarm button in the car).
//
// COVM: unchanged from B-EM. Analog Input 1 ("Bronze") has both its Present_Value
// and Status_Flags marked subscribable.
//
// To be a conformant BACnet device (Protocol_Revision 24) each object must
// expose its full set of REQUIRED properties. Most are generated by the stack
// (Object_Identifier, Object_Type, Status_Flags, Object_List, Protocol_*). The
// elevator objects are unusual: the stack stores only a handful of their
// properties (see the comment above BACnetStack_AddElevatorGroupObject /
// BACnetStack_AddLiftOrEscalatorObject in CASBACnetStackDLL.h) - EVERYTHING else
// must come from this file's Get* callbacks. Skip one and the stack does NOT
// error loudly: Car_Position/Car_Moving_Direction/Car_Door_Status/Passenger_Alarm
// on a Lift (and Operation_Direction/Passenger_Alarm on an Escalator) are the one
// place the stack DOES refuse to invent a value (value-not-initialized) - but
// Out_Of_Service silently reports false and Object_Name silently reports
// "undefined" if you forget it, exactly the duplicate-name trap the B-AAC
// example's callback comments warn about. Walk every callback below when you add
// an object, and diff its required properties against a working one.
//
// Interactive keys (handled by the shared helper): h = help, q = quit,
// up/down = nudge Analog Input 1 by +/-1.1 (also the F-COVM demonstration), d =
// DM-DDB-A: broadcast a demo Who-Is on demand (see docs/menu-keys.md - the same
// key B-LS claims for its own remote-discovery demo). There is no key to fire
// the Lift's Passenger_Alarm alarm - it is simulated on a 30-second timer
// instead. Command line: --port <n>, --deviceID <n>.
//
// All the UDP/stack plumbing lives in common/CASExampleHelper so this file can
// stay focused on the BACnet logic.
// =============================================================================

#include "CASExampleHelper.h"
#include "CASBACnetStackExampleConstants.h"
#include "CASBACnetStackAdapter.h" // the CAS BACnet Stack C API (BACnetStack_*); call
                                    // LoadBACnetFunctions() before any BACnetStack_* call -
                                    // see the top of main() below.

#include <stdio.h>
#include <string.h>
#include <time.h> // time() - the Passenger_Alarm simulation timer

#if defined(_WIN32)
#include <windows.h> // Sleep()
#else
#include <unistd.h>  // usleep()
#endif

using namespace CASBACnetStackExampleConstants;

// -----------------------------------------------------------------------------
// 1. Example + device configuration
// -----------------------------------------------------------------------------
static const char* APP_NAME = "BACnet B-AEC (Advanced Elevator Controller) Example - C++";
static const char* APP_VERSION = "1.0.0";

// The device instance. BACnet requires this to be configurable, so it defaults
// to 389014 and can be overridden on the command line with --deviceID.
static uint32_t g_deviceInstance = 389014;

// ---- Device identity: CHANGE ALL OF THIS BEFORE YOU SHIP --------------------
// Everything in this block is read by clients and shown to the operator in every
// discovery tool on the network. Left as-is, your product will appear on a real
// site announcing itself as a Chipkin demo. None of it is cosmetic:
// Object_Name must be unique across the BACnet internetwork, and Model_Name /
// Vendor_Identifier are what a building operator uses to identify your device.
// -----------------------------------------------------------------------------

// Your BACnet Vendor Identifier. 389 = Chipkin Automation Systems; change this
// to YOUR company's vendor ID before shipping a product. Vendor IDs are assigned
// by ASHRAE - request one (free) at https://bacnet.org/assigned-vendor-ids/.
// Update VENDOR_NAME below to match.
static const uint32_t VENDOR_IDENTIFIER = 389;

// This device's Object_Name. CHANGE THIS: Object_Name MUST BE UNIQUE ACROSS THE
// BACNET INTERNETWORK - two devices answering with the same Object_Name is a
// spec violation, and nothing on the wire flags it as an error (see TUTORIAL.md's
// silent-failure table). Here it is a compile-time constant, which is fine for a
// single tutorial instance; a real product must make it per-unit configurable -
// from a serial number, DIP switches, a config file, or a `--deviceName`
// command-line argument - not hard-coded the way this example does it.
static const char* DEVICE_NAME = "Rainbow";
static const char* DEVICE_DESCRIPTION =
    "Chipkin CAS BACnet Stack example - B-AEC (Advanced Elevator Controller) profile. "
    "DS-RP/RPM/WP/WPM-B, DS-COV-B, DS-COVM-B, intrinsic alarming "
    "(AE-N-I-B / AE-ACK-B / AE-INFO-B), AE-EL-I-B (Event Log), SCHED-I-B (Schedule), "
    "DM-DDB-A, DM-DCC-B, DM-TS-B, DM-RD-B, DM-OCD-B (CreateObject/DeleteObject), "
    "DM-BR-B (Backup/Restore). Commands three outputs and the Elevator Group's landing calls."; // CHANGE THIS: what YOUR device actually is.

// Device identity strings (read by clients, and used to populate I-Am).
static const char* VENDOR_NAME = "Chipkin Automation Systems"; // CHANGE THIS: must match VENDOR_IDENTIFIER above.
static const char* MODEL_NAME = "CAS BACnet Stack Example - B-AEC"; // CHANGE THIS: your model designation - what a building operator reads to identify your device.

// DeviceCommunicationControl password. A management station may include a password
// with a DeviceCommunicationControl request; the device accepts the command only if
// it matches. Set to NULL/empty to accept any request (no password required).
// Change this to your device's secret before shipping. It crosses the wire in
// PLAINTEXT - a guard against accidents, not a security boundary.
static const char* DCC_PASSWORD = "";  // "" = no password required
static const char* FIRMWARE_REVISION = "1.0.0"; // CHANGE THIS: your real firmware version - wire it to your build.
static const char* APPLICATION_SOFTWARE_VERSION = "1.0.0"; // CHANGE THIS: your real application software version - wire it to your build.

// The base sensor objects (all instance 1) and their colour names. Every example
// in the series carries these three plus the Network Port - see docs/colour-table.md.
static const uint32_t ANALOG_INPUT_INSTANCE = 1;       // "Bronze"
static const uint32_t BINARY_INPUT_INSTANCE = 1;       // "Emerald"
static const uint32_t MULTI_STATE_INPUT_INSTANCE = 1;  // "Hot Pink"
static const uint32_t MULTI_STATE_INPUT_NUMBER_OF_STATES = 3;

// -----------------------------------------------------------------------------
// F-OUTPUTS (canonical in B-SA-CPP, copied here verbatim): three commandable
// output objects - the NEW part of this profile versus B-EM. Colour names per
// docs/colour-table.md.
// -----------------------------------------------------------------------------
static const uint32_t ANALOG_OUTPUT_INSTANCE = 1;        // "Chartreuse"
static const uint32_t BINARY_OUTPUT_INSTANCE = 1;        // "Fuchsia"
static const uint32_t MULTI_STATE_OUTPUT_INSTANCE = 1;   // "Indigo"
static const uint32_t MULTI_STATE_OUTPUT_NUMBER_OF_STATES = 3;
static const uint32_t BACNET_PRIORITY_ARRAY_SIZE = 16;

// A BACnet commandable value: a 16-slot Priority_Array plus a Relinquish_Default.
// Each slot is either null (relinquished) or holds a commanded value. A real
// device would map the resolved Present_Value onto its physical output; here we
// just store the commands. The values are kept as double and cast per object
// type (REAL for AO, 0/1 for BO, state number for MSO).
struct Commandable {
    bool isSet[16];          // is slot i (1..16) commanded?
    double value[16];        // the commanded value at slot i
    double relinquishDefault; // used when every slot is null
};

// The { { false }, { 0 }, default } initializer zero-fills all 16 slots of isSet
// and value (C++ aggregate rules: the remaining elements are value-initialized),
// so every priority slot starts null and Present_Value reports relinquishDefault.
static Commandable g_analogOutput = { { false }, { 0 }, 20.0 }; // setpoint, default 20.0 C
static Commandable g_binaryOutput = { { false }, { 0 }, 0.0 };  // default inactive (0)
static Commandable g_multiStateOutput = { { false }, { 0 }, 1.0 }; // default state 1

// A WriteProperty to a commandable Present_Value carries a priority 1..16. When a
// client omits it, BACnet uses 16 (the lowest priority) - so normalise anything
// out of range to 16, matching the stack's own behaviour.
static uint8_t EffectivePriority(uint8_t priority) {
    return (priority >= 1 && priority <= BACNET_PRIORITY_ARRAY_SIZE) ? priority : 16;
}

// Store a commanded value at a priority slot (a WriteProperty of a value).
static void CommandWrite(Commandable* c, uint8_t priority, double value) {
    const uint8_t p = EffectivePriority(priority);
    c->isSet[p - 1] = true;
    c->value[p - 1] = value;
}

// Relinquish (clear) a priority slot - i.e. a WriteProperty of NULL.
static void CommandRelinquish(Commandable* c, uint8_t priority) {
    const uint8_t p = EffectivePriority(priority);
    c->isSet[p - 1] = false;
}

// Resolve which Commandable an (objectType, objectInstance) maps to, or NULL.
static Commandable* GetCommandable(uint16_t objectType, uint32_t objectInstance) {
    if (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE) {
        return &g_analogOutput;
    }
    if (objectType == OBJECT_TYPE_BINARY_OUTPUT && objectInstance == BINARY_OUTPUT_INSTANCE) {
        return &g_binaryOutput;
    }
    if (objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT && objectInstance == MULTI_STATE_OUTPUT_INSTANCE) {
        return &g_multiStateOutput;
    }
    return NULL;
}

// Is this read a single Priority_Array element (Priority_Array[1..16])? If so,
// report whether that slot is commanded (*slotIsSet) and its value (*slotValue).
// The typed Get callbacks use this to serve a commandable object's Priority_Array
// and to let the stack compute Present_Value from the highest non-null slot.
static bool ReadPrioritySlot(const Commandable* c, uint32_t propertyIdentifier,
                             bool useArrayIndex, uint32_t propertyArrayIndex,
                             bool* slotIsSet, double* slotValue) {
    if (propertyIdentifier != PROPERTY_IDENTIFIER_PRIORITY_ARRAY || !useArrayIndex ||
        propertyArrayIndex < 1 || propertyArrayIndex > BACNET_PRIORITY_ARRAY_SIZE) {
        return false;
    }
    *slotIsSet = c->isSet[propertyArrayIndex - 1];
    *slotValue = c->value[propertyArrayIndex - 1];
    return true;
}

// -----------------------------------------------------------------------------
// DM-OCD-B (CreateObject/DeleteObject, the headline addition over B-EC): the
// device advertises Analog Value as creatable (BACnetStack_SetObjectTypeCreatable,
// set up in main()) and the CreateObject/DeleteObject callbacks below let a
// client actually add/remove Analog Value instances at runtime - it is the
// ONLY creatable object type in this profile; every other object type in this
// file is added once at start-up the same way every other example in the
// series does. This repository is CANONICAL for F-OCD.
//
// A created Analog Value lives ENTIRELY in this table, not in the stack's own
// per-object storage. BACnetStack_RegisterCallbackCreateObject's doc comment in
// CASBACnetStackDLL.h (pin abd4cee1) is explicit about why: "it fires BEFORE
// the object exists in the stack's database. Allocate host-side storage; do
// not try to read the object back from the stack yet." - so this array IS
// that host-side storage, and the Analog Value branches added to
// GetPropertyReal/SetPropertyReal further down consult only it.
// -----------------------------------------------------------------------------
static const uint32_t MAX_CREATED_ANALOG_VALUES = 8; // an arbitrary, documented cap - see CreateObject's "no space" path below
struct CreatedAnalogValue {
    bool inUse;
    uint32_t objectInstance;
    float presentValue; // Present_Value has NO stack default for a non-commandable Analog Value
                         // (property-profile-reference.md) - this file owns it entirely.
};
static CreatedAnalogValue g_createdAnalogValues[MAX_CREATED_ANALOG_VALUES];

static CreatedAnalogValue* FindCreatedAnalogValue(const uint32_t objectInstance) {
    for (uint32_t i = 0; i < MAX_CREATED_ANALOG_VALUES; ++i) {
        if (g_createdAnalogValues[i].inUse && g_createdAnalogValues[i].objectInstance == objectInstance) {
            return &g_createdAnalogValues[i];
        }
    }
    return NULL;
}

static CreatedAnalogValue* FindFreeAnalogValueSlot(void) {
    for (uint32_t i = 0; i < MAX_CREATED_ANALOG_VALUES; ++i) {
        if (!g_createdAnalogValues[i].inUse) {
            return &g_createdAnalogValues[i];
        }
    }
    return NULL;
}

// The Network Port object - every BACnet device must have one. It represents
// the BACnet/IP port this device communicates on.
static const uint32_t NETWORK_PORT_INSTANCE = 1;       // "Vermilion"
static const uint32_t MAX_APDU_LENGTH = 1476;          // BACnet/IP APDU length

// BACnet/IP addressing the Network Port reports. The IP address and subnet mask
// are filled in at start-up from the host's primary interface; the gateway is
// left unset (0.0.0.0) for this example. The stack also uses IP_Address +
// BACnet_IP_UDP_Port to build the port's MAC_Address automatically.
static uint8_t g_ipAddress[4] = { 0, 0, 0, 0 };
static uint8_t g_ipSubnetMask[4] = { 0, 0, 0, 0 };
static uint8_t g_ipDefaultGateway[4] = { 0, 0, 0, 0 };
static uint16_t g_bacnetIpUdpPort = 47808;

// Analog Input 1's live present value (degrees Celsius). Starts at 21.5 and is
// nudged by the up/down arrow keys - this is also the F-COVM demonstration
// value: a SubscribeCOVPropertyMultiple naming Present_Value AND Status_Flags
// fires one multi-property notification when this changes.
static float g_analogInput1Value = 21.5f;

// -----------------------------------------------------------------------------
// F-ELEVATOR: object-type / property-identifier numbers not already in
// CASBACnetStackExampleConstants.h (verified against BACnetObjectType.h /
// BACnetPropertyIdentifier.h at the pin abd4cee1, 6.0.21).
// -----------------------------------------------------------------------------
static const uint16_t OBJECT_TYPE_ELEVATOR_GROUP = 57;
static const uint16_t OBJECT_TYPE_ESCALATOR = 58;
static const uint16_t OBJECT_TYPE_LIFT = 59;
static const uint16_t OBJECT_TYPE_POSITIVE_INTEGER_VALUE = 48;

static const uint32_t PROPERTY_IDENTIFIER_ASSIGNED_LANDING_CALLS = 447;
static const uint32_t PROPERTY_IDENTIFIER_CAR_DOOR_STATUS = 450;
static const uint32_t PROPERTY_IDENTIFIER_CAR_MOVING_DIRECTION = 457;
static const uint32_t PROPERTY_IDENTIFIER_CAR_POSITION = 458;
static const uint32_t PROPERTY_IDENTIFIER_ESCALATOR_MODE = 462;
static const uint32_t PROPERTY_IDENTIFIER_FAULT_SIGNALS = 463;
static const uint32_t PROPERTY_IDENTIFIER_GROUP_MODE = 467;
static const uint32_t PROPERTY_IDENTIFIER_LANDING_CALL_CONTROL = 471; // NEW vs B-EM: now writable
static const uint32_t PROPERTY_IDENTIFIER_LANDING_CALLS = 470;
static const uint32_t PROPERTY_IDENTIFIER_LANDING_DOOR_STATUS = 472;
static const uint32_t PROPERTY_IDENTIFIER_OPERATION_DIRECTION = 477;
static const uint32_t PROPERTY_IDENTIFIER_PASSENGER_ALARM = 478;
static const uint32_t PROPERTY_IDENTIFIER_POWER_MODE = 479;
static const uint32_t PROPERTY_IDENTIFIER_REGISTERED_CAR_CALL = 480;

// -- F-TIMESYNC: Local_Date / Local_Time property identifiers (Device object) --
static const uint32_t PROPERTY_IDENTIFIER_LOCAL_DATE = 56;
static const uint32_t PROPERTY_IDENTIFIER_LOCAL_TIME = 57;

// BACnetLiftCarDirectionEnum (BACnetLiftCarDirection.h)
static const uint32_t LIFT_CAR_DIRECTION_UP = 3;
// BACnetDoorStatusEnum (BACnetDoorStatus.h) - shared with Access Door / the Lift's
// per-door Car_Door_Status and Landing_Door_Status arrays.
static const uint32_t DOOR_STATUS_CLOSED = 0;
// BACnetLiftGroupModeEnum (BACnetLiftGroupMode.h)
static const uint32_t LIFT_GROUP_MODE_NORMAL = 1;
// BACnetEscalatorOperationDirectionEnum (BACnetEscalatorOperationDirection.h)
static const uint32_t ESCALATOR_OPERATION_DIRECTION_UP_RATED_SPEED = 2;
// BACnetEscalatorModeEnum (BACnetEscalatorMode.h)
static const uint32_t ESCALATOR_MODE_UP = 2;

// --- F-ELEVATOR: Elevator Group 1 "Maroon", Lift 1 "Mauve", Escalator 1 "Mint" -
// Maroon groups Mauve (isGroupOfLifts = true); Mint is NOT part of an Elevator
// Group of Lifts, so it is added with the spec's "no reference" sentinel
// (4194303, NETWORK_PORT_REFERENCE_PORT_NONE reused - the same constant serves
// both "no reference" sentinels in the stack API).
static const uint32_t ELEVATOR_GROUP_INSTANCE = 1;      // "Maroon"
static const uint32_t LIFT_INSTANCE = 1;                // "Mauve"
static const uint32_t ESCALATOR_INSTANCE = 1;            // "Mint"
static const uint8_t ELEVATOR_GROUP_ID = 1;               // shared by Maroon and Mauve
static const uint8_t LIFT_INSTALLATION_ID = 1;
static const uint8_t ESCALATOR_GROUP_ID = 1;               // Mint is not grouped, but still carries an ID
static const uint8_t ESCALATOR_INSTALLATION_ID = 1;
// BACnetStack_AddElevatorGroupObject's machineRoomId names a Positive Integer
// Value object (instance 1, colour "Turquoise" per docs/colour-table.md) that
// THIS APPLICATION must add first - see the AddElevatorGroupObject call below
// for why its own doc comment's "the stack will create it" is not what happens.
static const uint32_t MACHINE_ROOM_ID_INSTANCE = 1;      // "Turquoise"
static const uint32_t MACHINE_ROOM_ID_VALUE = 100;        // an arbitrary plausible room id
static const uint32_t LIFT_NUMBER_OF_DOORS = 1;            // this Lift has one door (index 1)

// Simulated Lift state (a real device would read this from the elevator
// controller's own bus). Car_Position is a floor number (1-based); the car sits
// at floor 3, doors closed, not moving.
static const uint8_t LIFT_CAR_POSITION = 3;
static bool g_liftOutOfService = false;
// The alarm source: Passenger_Alarm is not writable through any API in this
// stack (a real installation drives it from hardware) - see the file header
// comment for how this example simulates it on a timer instead of a key/write.
static bool g_liftPassengerAlarm = false;

static bool g_escalatorOutOfService = false;
static bool g_escalatorPassengerAlarm = false;
static bool g_escalatorPowerMode = true; // powered on

// --- NEW vs B-EM: Elevator Group 1 "Maroon"'s Landing_Call_Control -----------
// The one queued/commanded landing call this example remembers, so a read of
// Landing_Call_Control (or Landing_Calls) after a WriteProperty reflects what was
// last commanded. A real installation would forward the call to its elevator
// controller instead of just remembering it. hasQueuedCall=false means "no call
// commanded (or its door was answered) right now" - a legitimate empty answer.
struct QueuedLandingCall {
    bool hasQueuedCall = false;
    uint8_t floorNumber = 0;
    uint8_t commandChoice = 0;
    uint32_t direction = 0;
    uint8_t destination = 0;
};
static QueuedLandingCall g_queuedLandingCall;

// --- AE-N-I-B: Notification Class 1 "Crimson" routes Mauve's Passenger_Alarm --
static const uint32_t NOTIFICATION_CLASS_INSTANCE = 1;     // "Crimson"
// Notification priorities for the three transitions (lower = more urgent). The
// to-fault priority is supplied for completeness, but this example's
// ChangeOfState algorithm has no fault source, so to-fault is left disabled below.
static const uint8_t NC_PRIORITY_TO_OFFNORMAL = 100;
static const uint8_t NC_PRIORITY_TO_FAULT = 50;
static const uint8_t NC_PRIORITY_TO_NORMAL = 200;

// Where Mauve's alarms are sent. See B-AAC's main.cpp for the full rationale -
// this example seeds the ADDRESS form, defaulting to the LOCAL SUBNET BROADCAST
// with UNCONFIRMED notifications, so any BACnet client on the subnet sees the
// alarm without us knowing its address ahead of time. Unlike B-AAC (AE-CRL-B),
// this profile's Recipient_List is NOT made writable (out of scope for B-EC's
// required BIBBs) - so the recipient seeded here is the only one, for the life
// of the process.
static const uint32_t RECIPIENT_PROCESS_IDENTIFIER = 1;
static const bool RECIPIENT_USE_BROADCAST = true;
static uint8_t RECIPIENT_IP[4] = { 0, 0, 0, 0 };  // used when not broadcasting

// How often (seconds of uptime) the simulated Passenger_Alarm condition flips.
// A real device would drive g_liftPassengerAlarm from an actual alarm input.
static const time_t PASSENGER_ALARM_SIMULATION_PERIOD_SECONDS = 30;

// -----------------------------------------------------------------------------
// AE-EL-I-B (F-EVENTLOG, NEW vs B-EC): Event Log 1 "Beige". Pattern copied
// byte-for-byte from B-ALSC-CPP, canonical for this feature. Every property
// BACnetStack_AddEventLogObject exposes (Enable, Buffer_Size, Record_Count,
// Total_Record_Count, Log_Buffer, ...) is held by the stack itself, not served
// through this file's Get*/Set* callbacks - see the doc comment on
// BACnetStack_AddEventLogObject in CASBACnetStackDLL.h at the pin abd4cee1.
// This example does not enable the optional Start_Time/Stop_Time (cl. 12.27).
// -----------------------------------------------------------------------------
static const uint16_t OBJECT_TYPE_EVENT_LOG = 25;            // "Beige"
static const uint32_t EVENT_LOG_INSTANCE = 1;                // "Beige"
static const uint32_t EVENT_LOG_MAX_BUFFER_SIZE = 100;       // records held in memory
static const uint32_t PROPERTY_IDENTIFIER_ENABLE = 133;      // Event Log 1 (Beige) Log_Enable

// -----------------------------------------------------------------------------
// SCHED-I-B (F-SCHED-I, NEW vs B-EC): Schedule 1 "Saffron" + Calendar 1
// "Cream". Pattern copied byte-for-byte from B-AAC-CPP, canonical for this
// feature (B-ALSC's copy of the same pattern was used as the direct template
// here, since it is the more recent sibling). Saffron drives Analog Output 1
// "Chartreuse" (already part of this profile's F-OUTPUTS base) at
// SCHEDULE_WRITE_PRIORITY whenever a Weekly_Schedule or Exception_Schedule
// entry is active; Schedule_Default applies the rest of the time.
// -----------------------------------------------------------------------------
static const uint16_t OBJECT_TYPE_SCHEDULE = 17;
static const uint16_t OBJECT_TYPE_CALENDAR = 6;
static const uint32_t SCHEDULE_INSTANCE = 1;                 // "Saffron"
static const uint32_t CALENDAR_INSTANCE = 1;                 // "Cream"
static const uint8_t SCHEDULE_WRITE_PRIORITY = 8;            // mid-range: below manual overrides at 1-7
static const float SCHEDULE_DEFAULT_VALUE = 20.0f;           // Chartreuse's steady-state setpoint
static const float SCHEDULE_DEMO_VALUE = 75.0f;              // the value a scheduled/demo transition applies
static const float SCHEDULE_EXCEPTION_VALUE = 5.0f;          // the value the one-off exception applies

// -----------------------------------------------------------------------------
// DM-BR-B (F-BACKUP, NEW vs B-EC): File 1 "Ivory" (the backup/restore payload
// carrier) plus the four backup/restore lifecycle callbacks. Pattern copied
// byte-for-byte from B-ACC-CPP, canonical for this feature - including its
// documented, wire-verified gaps (see TODO.md: File_Size answering
// read-access-denied despite being served here, and a live AtomicReadFile
// aborting during an active backup session; both carried forward, not
// silently avoided).
// -----------------------------------------------------------------------------
static const uint16_t OBJECT_TYPE_FILE = 10;
static const uint32_t FILE_INSTANCE = 1;                     // "Ivory"
static const uint32_t FILE_ACCESS_METHOD_STREAM = 1;
static const uint32_t FILE_MAX_SIZE = 4096;
static uint8_t g_fileData[FILE_MAX_SIZE];
static uint32_t g_fileDataLength = 0;
static bool g_fileArchive = false;
static const uint32_t PROPERTY_IDENTIFIER_FILE_SIZE = 42;
static const uint32_t PROPERTY_IDENTIFIER_ARCHIVE = 13;
static const uint32_t PROPERTY_IDENTIFIER_READ_ONLY = 99;

// -----------------------------------------------------------------------------
// 2. Property "get" callbacks
//
// The stack calls these when a client reads a property. For each data type the
// stack uses a separate callback. We return true (and fill *value) when we
// recognise the (object, property) pair, and false otherwise.
//
// THE errorCode OUT-PARAMETER. Every Get callback ends with uint32_t* errorCode.
// The stack PRESETS it to success (84) before the call, and reads it only if you
// return false. That gives a declining callback two distinct meanings:
//
//   1. return false and LEAVE errorCode ALONE  -> "I have no opinion on this
//      property." The stack falls back to its own handling (see below).
//   2. return false and SET *errorCode         -> "This read fails, with THIS
//      BACnet error." The client gets exactly that Error-PDU.
//
// WHAT false-WITHOUT-AN-ERROR-CODE ACTUALLY DOES. It does NOT reliably produce a
// BACnet error. The stack errors only for the handful of properties it refuses
// to invent - on this device that includes Car_Position, Car_Moving_Direction,
// Car_Door_Status and Passenger_Alarm on the Lift, and Operation_Direction and
// Passenger_Alarm on the Escalator (value-not-initialized), on top of the usual
// Present_Value / Number_Of_States / Relinquish_Default / Local_Date / Local_Time
// / a Network Port's APDU_Length. For EVERYTHING ELSE a false return means the
// stack SILENTLY SUBSTITUTES a default (Object_Name -> "undefined", Units ->
// no-units, otherwise a datatype zero-value) - load-bearing for the required
// properties this application does not serve, so it is not "fixed" here. Set
// *errorCode ONLY where this device knows the read is wrong; the one such case
// below is State_Text with an out-of-range array index.
//
// ADDING AN OBJECT? READ THIS FIRST. These callbacks are not uniformly strict -
// see the file header comment above for the duplicate-Object_Name trap. Walk
// every callback below when you add an instance, then read back every required
// property of the new object and diff it against a working one.
// -----------------------------------------------------------------------------

// REAL (floating point) - the Analog Input's Present_Value.
bool GetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     float* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode; // see "THE errorCode OUT-PARAMETER" above: every catch-all here declines without naming an error
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_ANALOG_INPUT &&
        objectInstance == ANALOG_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // ON REAL HARDWARE: return the live sensor reading here. Read it from a
        // cached variable that your hardware updates (as g_analogInput1Value is),
        // NOT directly from a slow/blocking device: this callback runs on the
        // BACnetStack_Tick() thread, so blocking it delays all BACnet processing.
        *value = g_analogInput1Value;
        return true;
    }
    // F-OUTPUTS (NEW vs B-EM): Analog Output 1 (Chartreuse) - commandable. Serve
    // its Priority_Array slots and Relinquish_Default; a null slot declines so
    // GetPropertyBool below answers "slot is null".
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_ANALOG_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (float)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (float)c->relinquishDefault;
            return true;
        }
    }
    // DM-OCD-B: Present_Value of a runtime-created Analog Value (see the
    // CreateObject/DeleteObject callbacks and g_createdAnalogValues above).
    if (objectType == OBJECT_TYPE_ANALOG_VALUE && propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        const CreatedAnalogValue* av = FindCreatedAnalogValue(objectInstance);
        if (av != NULL) {
            *value = av->presentValue;
            return true;
        }
    }
    return false;
}

// ENUMERATED - the Binary Input's Present_Value, the Analog Input's Units, and
// every elevator-family enumerated property (Car_Moving_Direction, Operation_Direction,
// Group_Mode, Escalator_Mode, Car_Door_Status).
bool GetPropertyEnumerated(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           uint32_t* value, const bool useArrayIndex,
                           const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_BINARY_INPUT &&
        objectInstance == BINARY_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // active
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POLARITY) {
            *value = POLARITY_NORMAL; // required property of a Binary Input
            return true;
        }
    }
    // F-OUTPUTS (NEW vs B-EM): Binary Output 1 (Fuchsia) - commandable Present_Value
    // plus its required Polarity.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_BINARY_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (uint32_t)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (uint32_t)c->relinquishDefault;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POLARITY) {
            *value = POLARITY_NORMAL; // required property of a Binary Output
            return true;
        }
    }
    // Units is REQUIRED on an Analog Input AND on an Analog Output. Serve BOTH -
    // an Analog Output without this silently reads back "no units" (see B-SA's
    // main.cpp for the full explanation of why a false return here is not safe
    // to assume harmless).
    if (propertyIdentifier == PROPERTY_IDENTIFIER_UNITS &&
        ((objectType == OBJECT_TYPE_ANALOG_INPUT && objectInstance == ANALOG_INPUT_INSTANCE) ||
         (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE))) {
        *value = ENGINEERING_UNITS_DEGREES_CELSIUS;
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT &&
        objectInstance == NETWORK_PORT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_MODE) {
        *value = BACNET_IP_MODE_NORMAL; // not foreign-device, not BBMD
        return true;
    }

    // --- F-ELEVATOR ----------------------------------------------------------
    if (objectType == OBJECT_TYPE_ELEVATOR_GROUP && objectInstance == ELEVATOR_GROUP_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_GROUP_MODE) {
        // Enabled automatically because isGroupOfLifts=true was passed to
        // BACnetStack_AddElevatorGroupObject (required only in that case).
        *value = LIFT_GROUP_MODE_NORMAL;
        return true;
    }
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_CAR_MOVING_DIRECTION) {
            // Required, NO STACK DEFAULT - an unserved read fails value-not-initialized.
            *value = LIFT_CAR_DIRECTION_UP;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_CAR_DOOR_STATUS &&
            useArrayIndex && propertyArrayIndex >= 1 && propertyArrayIndex <= LIFT_NUMBER_OF_DOORS) {
            // Required, per-door array (this Lift has one door - see the
            // "THE DOOR COUNT IS NOT STORED BY THE STACK" note in
            // CASBACnetStackDLL.h: the application alone owns the door count,
            // by simply answering only door indices it recognises).
            *value = DOOR_STATUS_CLOSED;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_ESCALATOR && objectInstance == ESCALATOR_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_OPERATION_DIRECTION) {
            // Required, NO STACK DEFAULT.
            *value = ESCALATOR_OPERATION_DIRECTION_UP_RATED_SPEED;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_ESCALATOR_MODE) {
            // Optional; enabled below with SetPropertyEnabled.
            *value = ESCALATOR_MODE_UP;
            return true;
        }
    }
    return false;
}

// UNSIGNED INTEGER - the Multi-State Input's Present_Value, the Device's
// Vendor_Identifier, the Lift's Car_Position (and its door-indexed lists), and
// the machine-room Positive Integer Value's Present_Value.
bool GetPropertyUnsignedInteger(const uint32_t deviceInstance, const uint16_t objectType,
                                const uint32_t objectInstance, const uint32_t propertyIdentifier,
                                uint32_t* value, const bool useArrayIndex,
                                const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // state 1 (valid range is 1..Number_Of_States)
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_STATES) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES; // required property
            return true;
        }
        // State_Text is an array. The stack asks for its LENGTH here (array
        // index 0) before reading each element via GetPropertyCharString.
        if (propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT &&
            useArrayIndex && propertyArrayIndex == 0) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance &&
        propertyIdentifier == PROPERTY_IDENTIFIER_VENDOR_IDENTIFIER) {
        *value = VENDOR_IDENTIFIER;
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_APDU_LENGTH) {
            *value = MAX_APDU_LENGTH;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_REFERENCE_PORT) {
            *value = NETWORK_PORT_REFERENCE_PORT_NONE;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_UDP_PORT) {
            *value = g_bacnetIpUdpPort;
            return true;
        }
    }

    // --- F-ELEVATOR ----------------------------------------------------------
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_CAR_POSITION) {
        // Required, NO STACK DEFAULT.
        *value = LIFT_CAR_POSITION;
        return true;
    }
    // Every one of the Lift's per-door BACnetARRAY properties (required
    // Car_Door_Status, optional Assigned_Landing_Calls/Registered_Car_Call/
    // Landing_Door_Status) is asked for its LENGTH here first (array index 0,
    // exactly like Multi-State Input 1's State_Text above) before the stack
    // reads/pages the individual elements - through GetPropertyEnumerated for
    // Car_Door_Status, or through the matching
    // GetSequenceLift*/GetListElevatorGroupLandingCallStatus callback for the
    // other three. Skipping this made every one of those reads fail with
    // BuildArrayProperty "failed to get the size of the array" - caught by
    // testing against a real client, not by reading the doc comment, which
    // does not mention this length-query step at all for these properties.
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE &&
        useArrayIndex && propertyArrayIndex == 0 &&
        (propertyIdentifier == PROPERTY_IDENTIFIER_CAR_DOOR_STATUS ||
         propertyIdentifier == PROPERTY_IDENTIFIER_ASSIGNED_LANDING_CALLS ||
         propertyIdentifier == PROPERTY_IDENTIFIER_REGISTERED_CAR_CALL ||
         propertyIdentifier == PROPERTY_IDENTIFIER_LANDING_DOOR_STATUS)) {
        *value = LIFT_NUMBER_OF_DOORS;
        return true;
    }
    // The Positive Integer Value the Elevator Group's Machine_Room_ID names
    // (added explicitly by this application - see the AddObject call right
    // before AddElevatorGroupObject below). Present_Value is required with no
    // stack default; must be served or the read fails.
    if (objectType == OBJECT_TYPE_POSITIVE_INTEGER_VALUE &&
        objectInstance == MACHINE_ROOM_ID_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        *value = MACHINE_ROOM_ID_VALUE;
        return true;
    }

    // F-OUTPUTS (NEW vs B-EM): Multi-State Output 1 (Indigo) - commandable state
    // number driven through the Priority_Array. Serve the array slots,
    // Relinquish_Default, and the required Number_Of_States.
    {
        const Commandable* c = GetCommandable(objectType, objectInstance);
        if (c != NULL && objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT) {
            bool slotIsSet = false;
            double slotValue = 0.0;
            if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                                 &slotIsSet, &slotValue)) {
                if (!slotIsSet) {
                    return false;
                }
                *value = (uint32_t)slotValue;
                return true;
            }
            if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
                *value = (uint32_t)c->relinquishDefault;
                return true;
            }
            if (propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_STATES) {
                *value = MULTI_STATE_OUTPUT_NUMBER_OF_STATES;
                return true;
            }
        }
    }
    // DM-BR-B: Ivory (File 1) - File_Size is REQUIRED with no stack default
    // (property-profile-reference.md: "no default...value-not-initialized
    // until a callback or WriteProperty supplies one"). NOTE (carried forward
    // from B-ACC-CPP's TODO.md, part of stack issue #2046): a live ReadProperty
    // of this property answers Error(object: read-access-denied) despite being
    // served correctly here - see TODO.md.
    if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_FILE_SIZE) {
        *value = g_fileDataLength;
        return true;
    }
    return false;
}

// BOOLEAN - Out_Of_Service is a required property of every input object, the
// Network Port, the Lift and the Escalator; Passenger_Alarm (required, no stack
// default) on the Lift and Escalator; Power_Mode (optional) on the Escalator;
// (NEW vs B-EM) also the Priority_Array null-slot query for the three
// commandable outputs.
bool GetPropertyBool(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     bool* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }

    // F-OUTPUTS (NEW vs B-EM): the stack asks "is this Priority_Array slot null?"
    // with the boolean getter. Answer true (1) for a relinquished slot, false (0)
    // for a commanded one - this is how the stack knows which slots to skip when
    // computing Present_Value.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && propertyIdentifier == PROPERTY_IDENTIFIER_PRIORITY_ARRAY &&
        useArrayIndex && propertyArrayIndex >= 1 &&
        propertyArrayIndex <= BACNET_PRIORITY_ARRAY_SIZE) {
        *value = !c->isSet[propertyArrayIndex - 1];
        return true;
    }

    // --- F-ELEVATOR ----------------------------------------------------------
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PASSENGER_ALARM) {
            // Required, NO STACK DEFAULT. This is the alarm source (see the file
            // header comment) - simulated on a timer, never written.
            *value = g_liftPassengerAlarm;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE) {
            *value = g_liftOutOfService;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_ESCALATOR && objectInstance == ESCALATOR_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PASSENGER_ALARM) {
            *value = g_escalatorPassengerAlarm;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE) {
            *value = g_escalatorOutOfService;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POWER_MODE) {
            // Optional; enabled below with SetPropertyEnabled.
            *value = g_escalatorPowerMode;
            return true;
        }
    }

    // Out_Of_Service is a required property of every input AND output object,
    // and of the Network Port too. This example never takes anything out of
    // service: false.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE &&
        (objectType == OBJECT_TYPE_ANALOG_INPUT ||
         objectType == OBJECT_TYPE_BINARY_INPUT ||
         objectType == OBJECT_TYPE_MULTI_STATE_INPUT ||
         objectType == OBJECT_TYPE_ANALOG_OUTPUT ||
         objectType == OBJECT_TYPE_BINARY_OUTPUT ||
         objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT ||
         objectType == OBJECT_TYPE_NETWORK_PORT ||
         objectType == OBJECT_TYPE_SCHEDULE)) {
        *value = false;
        return true;
    }
    // SCHED-I-B: Cream (Calendar 1) - Present_Value (required, no stack
    // default): true when today matches an entry in Date_List. Date_List
    // cannot be populated through the customer API (stack issue #963, same
    // gap B-AAC/B-ALSC document), so this honestly always reports false
    // rather than fabricating a match - see TODO.md.
    if (objectType == OBJECT_TYPE_CALENDAR && objectInstance == CALENDAR_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        *value = false;
        return true;
    }
    // DM-BR-B: Ivory (File 1) - Archive (writable, DM-BR-B configuration-file
    // marker) and Read_Only are both REQUIRED with no stack default.
    if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_ARCHIVE) {
            *value = g_fileArchive;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_READ_ONLY) {
            *value = false; // this example's File is writable (AddFileObject isWritable=true)
            return true;
        }
    }
    return false;
}

// OCTET STRING - the Network Port's BACnet/IP addressing. The stack cannot know
// the host's IP, so the application must supply IP_Address and IP_Subnet_Mask
// (and IP_Default_Gateway). Each is four octets.
bool GetPropertyOctetString(const uint32_t deviceInstance, const uint16_t objectType,
                            const uint32_t objectInstance, const uint32_t propertyIdentifier,
                            uint8_t* value, uint32_t* valueElementCount,
                            const uint32_t maxElementCount, const bool useArrayIndex,
                            const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)errorCode;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance ||
        objectType != OBJECT_TYPE_NETWORK_PORT ||
        objectInstance != NETWORK_PORT_INSTANCE ||
        maxElementCount < 4) {
        return false;
    }
    const uint8_t* source = NULL;
    switch (propertyIdentifier) {
        case PROPERTY_IDENTIFIER_IP_ADDRESS:         source = g_ipAddress; break;
        case PROPERTY_IDENTIFIER_IP_SUBNET_MASK:     source = g_ipSubnetMask; break;
        case PROPERTY_IDENTIFIER_IP_DEFAULT_GATEWAY: source = g_ipDefaultGateway; break;
        default: return false;
    }
    memcpy(value, source, 4);
    *valueElementCount = 4;
    return true;
}

// Small helper: copy a C string into the stack's character-string buffer and
// set the element count + encoding. Returns true (so callers can `return`).
static bool ReturnCharacterString(const char* text, char* value,
                                  uint32_t* valueElementCount,
                                  const uint32_t maxElementCount,
                                  uint8_t* encodingType) {
    uint32_t length = (uint32_t)strlen(text);
    if (length > maxElementCount) {
        // Truncate SILENTLY to fit the stack's buffer - see B-AAC's main.cpp for
        // the full explanation of when this matters (STACK_OPTION_TARGET_EMBEDDED).
        length = maxElementCount;
    }
    memcpy(value, text, length);
    *valueElementCount = length;
    *encodingType = CHARACTER_STRING_ENCODING_UTF8;
    return true;
}

// CHARACTER STRING - Object_Name for each object, and the device Description.
bool GetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           char* value, uint32_t* valueElementCount,
                           const uint32_t maxElementCount, uint8_t* encodingType,
                           const bool useArrayIndex, const uint32_t propertyArrayIndex,
                           uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }

    // State_Text (optional) - one label per state of the Multi-State Input. It is
    // a BACnet array, so the stack asks for one element at a time by index
    // (1..Number_Of_States). Present_Value 1 -> "On", 2 -> "Off", 3 -> "Auto".
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT && useArrayIndex) {
        static const char* const stateText[] = { "On", "Off", "Auto" };
        if (propertyArrayIndex >= 1 && propertyArrayIndex <= MULTI_STATE_INPUT_NUMBER_OF_STATES) {
            return ReturnCharacterString(stateText[propertyArrayIndex - 1], value,
                                         valueElementCount, maxElementCount, encodingType);
        }
        // The one place in this file where naming an error is clearly right: the
        // client asked for State_Text[n] and this object has no element n.
        *errorCode = ERROR_CODE_INVALID_ARRAY_INDEX;
        return false;
    }

    // Object_Name - the colour name for each object.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OBJECT_NAME) {
        if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
            return ReturnCharacterString(DEVICE_NAME, value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ANALOG_INPUT && objectInstance == ANALOG_INPUT_INSTANCE) {
            return ReturnCharacterString("Bronze", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_BINARY_INPUT && objectInstance == BINARY_INPUT_INSTANCE) {
            return ReturnCharacterString("Emerald", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT && objectInstance == MULTI_STATE_INPUT_INSTANCE) {
            return ReturnCharacterString("Hot Pink", value, valueElementCount, maxElementCount, encodingType);
        }
        // F-OUTPUTS (NEW vs B-EM):
        if (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Chartreuse", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_BINARY_OUTPUT && objectInstance == BINARY_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Fuchsia", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT && objectInstance == MULTI_STATE_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Indigo", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
            return ReturnCharacterString("Vermilion", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ELEVATOR_GROUP && objectInstance == ELEVATOR_GROUP_INSTANCE) {
            return ReturnCharacterString("Maroon", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE) {
            return ReturnCharacterString("Mauve", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ESCALATOR && objectInstance == ESCALATOR_INSTANCE) {
            return ReturnCharacterString("Mint", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_POSITIVE_INTEGER_VALUE && objectInstance == MACHINE_ROOM_ID_INSTANCE) {
            return ReturnCharacterString("Turquoise", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NOTIFICATION_CLASS && objectInstance == NOTIFICATION_CLASS_INSTANCE) {
            return ReturnCharacterString("Crimson", value, valueElementCount, maxElementCount, encodingType);
        }
    }

    // The remaining strings are all on the Device object - its identity, read
    // by clients and used to populate the device's I-Am / object list.
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
        switch (propertyIdentifier) {
            case PROPERTY_IDENTIFIER_DESCRIPTION:
                return ReturnCharacterString(DEVICE_DESCRIPTION, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_VENDOR_NAME:
                return ReturnCharacterString(VENDOR_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_MODEL_NAME:
                return ReturnCharacterString(MODEL_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_FIRMWARE_REVISION:
                return ReturnCharacterString(FIRMWARE_REVISION, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_APPLICATION_SOFTWARE_VERSION:
                return ReturnCharacterString(APPLICATION_SOFTWARE_VERSION, value, valueElementCount, maxElementCount, encodingType);
            default:
                break;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// 2b. F-ELEVATOR list/sequence callbacks
//
// These are the four elevator-specific callbacks the stack calls to page
// through a property that is a LIST or a per-door ARRAY rather than a plain
// typed value - one call per element, walking rangeOption/offset/more until the
// application says there is nothing left. A monitor with nothing outstanding
// answers the first call with *more = false (an empty list is a completely
// legitimate, honest answer - it means "no calls right now").
//
// This device is READ-ONLY, so the FIFTH elevator callback,
// BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl - the one that
// would accept a client's landing-call command - is DELIBERATELY NOT registered.
// Leaving it unregistered (rather than registering a callback that rejects
// everything) is itself part of the "no write" guarantee: the stack has no
// registered receiver for that command, so LandingCallControl is never made
// writable to begin with. B-EC (Wave 3), which seeds from this repo, is where
// that callback gets wired up.
// -----------------------------------------------------------------------------

// FaultSignals (required on the Lift) - a list of active fault enumerations.
// A healthy Lift reports an empty list; this example never simulates a fault.
bool GetListOfEnumerations(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           const uint8_t rangeOption, const uint32_t rangeIndexOrSequenceNumber,
                           const bool rangeInPositiveDirection, uint32_t* enumeration, bool* more) {
    (void)rangeOption;
    (void)rangeIndexOrSequenceNumber;
    (void)rangeInPositiveDirection;
    (void)enumeration;
    if (deviceInstance != g_deviceInstance) {
        *more = false;
        return false;
    }
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_FAULT_SIGNALS) {
        *more = false; // no active faults
        return true;
    }
    *more = false;
    return false;
}

// LandingCalls / LandingCallControl (optional; enabled on Maroon because
// supportLandingCallStatus was passed to AddElevatorGroupObject) - the group's
// current landing-call queue. B-EM (read-only) ignored propertyIdentifier and
// always answered an empty list; THIS example (NEW vs B-EM) actually has
// something to report for LandingCallControl once SetElevatorGroupLandingCallControl
// (below) has recorded a commanded call - LandingCalls (the group's own queue,
// as opposed to what was just commanded) stays empty, a legitimate answer for a
// controller that has not modeled a separate group-side queue.
bool GetListElevatorGroupLandingCallStatus(const uint32_t deviceInstance, const uint32_t elevatorGroupInstance,
                                           const uint32_t propertyIdentifier, const uint8_t rangeOption,
                                           const uint32_t rangeIndexOrSequence, uint8_t* floorNumber,
                                           uint8_t* commandChoice, uint32_t* bacnetLiftCarDirection,
                                           uint8_t* destination, bool* useFloorText, char* floorText,
                                           uint32_t* floorTextLength, const uint32_t floorTextMaxLength,
                                           bool* more) {
    (void)rangeOption;
    (void)floorText;
    (void)floorTextLength;
    (void)floorTextMaxLength;
    if (deviceInstance != g_deviceInstance || elevatorGroupInstance != ELEVATOR_GROUP_INSTANCE) {
        *more = false;
        return false;
    }
    if (propertyIdentifier == PROPERTY_IDENTIFIER_LANDING_CALL_CONTROL &&
        g_queuedLandingCall.hasQueuedCall && rangeIndexOrSequence == 0) {
        *floorNumber = g_queuedLandingCall.floorNumber;
        *commandChoice = g_queuedLandingCall.commandChoice;
        *bacnetLiftCarDirection = g_queuedLandingCall.direction;
        *destination = g_queuedLandingCall.destination;
        *useFloorText = false;
        *more = false; // exactly one queued call reported
        return true;
    }
    *more = false; // no landing calls outstanding right now (or index exhausted)
    return true;
}

// AssignedLandingCalls (optional Lift property, enabled below) - the calls this
// car has been assigned to answer. Report one, for demonstration: the car is
// assigned to floor 5, direction up.
bool GetSequenceLiftAssignedLandingCall(const uint32_t deviceInstance, const uint32_t objectInstance,
                                        const uint32_t arrayIndexForDoor, const uint32_t offset,
                                        uint8_t* floorNumber, uint32_t* direction, bool* more) {
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1 /* this Lift's one door */) {
        *more = false;
        return false;
    }
    if (offset == 0) {
        *floorNumber = 5;
        *direction = LIFT_CAR_DIRECTION_UP;
        *more = false; // exactly one assigned call
        return true;
    }
    *more = false;
    return false;
}

// RegisteredCarCall (optional Lift property, enabled below) - the floor(s) a
// passenger has pressed inside the car. Report one, for demonstration: floor 5.
bool GetSequenceLiftRegisteredCarCall(const uint32_t deviceInstance, const uint32_t objectInstance,
                                      const uint32_t arrayIndexForDoor, const uint32_t offset,
                                      uint8_t* floorNumber, bool* more) {
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1) {
        *more = false;
        return false;
    }
    if (offset == 0) {
        *floorNumber = 5;
        *more = false; // exactly one registered car call
        return true;
    }
    *more = false;
    return false;
}

// LandingDoorStatus (optional Lift property, enabled below) - the door status at
// each floor the car serves. Report one, for demonstration: floor 3 (where the
// car currently sits), door closed.
bool GetSequenceLiftLandingDoorStatus(const uint32_t deviceInstance, const uint32_t objectInstance,
                                      const uint32_t arrayIndexForDoor, const uint32_t offset,
                                      uint8_t* floorNumber, uint32_t* doorStatus, bool* more) {
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1) {
        *more = false;
        return false;
    }
    if (offset == 0) {
        *floorNumber = LIFT_CAR_POSITION;
        *doorStatus = DOOR_STATUS_CLOSED;
        *more = false; // exactly one landing-door status entry reported
        return true;
    }
    *more = false;
    return false;
}

// -----------------------------------------------------------------------------
// 2c. SetElevatorGroupLandingCallControl (NEW vs B-EM) - the "controller" half
// of "Elevator Controller". B-EM deliberately left this UNREGISTERED (see its
// file header) so LandingCallControl was never made writable to begin with.
// Registering it here is what turns this device from a monitor into a
// controller: the stack decodes the WriteProperty's BACnetLandingCallStatus
// SEQUENCE and hands us the fields; we just remember the last one (a real
// installation would forward it to the elevator controller's own bus). Reads of
// Landing_Call_Control afterward come back through
// GetListElevatorGroupLandingCallStatus above.
// -----------------------------------------------------------------------------
bool SetElevatorGroupLandingCallControl(const uint32_t deviceInstance, const uint32_t elevatorGroupInstance,
                                        const uint8_t floorNumber, const uint8_t commandChoice,
                                        const uint32_t bacnetLiftCarDirection, const uint8_t destination,
                                        const bool useFloorText, const char* floorText,
                                        const uint16_t floorTextLength) {
    (void)useFloorText;
    (void)floorText;
    (void)floorTextLength;
    if (deviceInstance != g_deviceInstance || elevatorGroupInstance != ELEVATOR_GROUP_INSTANCE) {
        return false;
    }
    g_queuedLandingCall.hasQueuedCall = true;
    g_queuedLandingCall.floorNumber = floorNumber;
    g_queuedLandingCall.commandChoice = commandChoice;
    g_queuedLandingCall.direction = bacnetLiftCarDirection;
    g_queuedLandingCall.destination = destination;
    printf("WriteProperty: Elevator Group 1 (Maroon) Landing_Call_Control <- floor %u, "
           "direction %u, door %u\n", floorNumber, bacnetLiftCarDirection, destination);
    return true;
}

// -----------------------------------------------------------------------------
// 2d. DeviceCommunicationControl callback (DM-DCC-B)
//
// A management station sends DeviceCommunicationControl to tell a device to stop
// or resume communicating. This is NOT a write to any BACnet object property -
// it is a device-management service every profile above B-ASC must support. The
// stack runs the actual enable/disable state machine; this callback validates
// the optional password and logs. See B-AAC's main.cpp for the full explanation
// of the errorCode contract on this callback (it has NO fallback - every false
// return here MUST set *errorCode) and the Protocol_Revision >= 20 deprecation
// of plain "disable". PasswordAccepted (below) is shared with ReinitializeDevice.
// -----------------------------------------------------------------------------
static bool PasswordAccepted(const char* password, uint32_t passwordLength) {
    const uint32_t requiredLength = (uint32_t)strlen(DCC_PASSWORD);
    if (requiredLength == 0) {
        return true; // no password required
    }
    if (password == NULL || passwordLength != requiredLength) {
        return false;
    }
    unsigned diff = 0;
    for (uint32_t i = 0; i < requiredLength; ++i) {
        diff |= (unsigned)((unsigned char)password[i] ^ (unsigned char)DCC_PASSWORD[i]);
    }
    return diff == 0;
}

bool DeviceCommunicationControl(const uint32_t deviceInstance, const uint8_t enableDisable,
                                const char* password, const uint8_t passwordLength,
                                const bool useTimeDuration, const uint16_t timeDuration,
                                uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }

    const size_t requiredLength = strlen(DCC_PASSWORD);
    if (requiredLength > 0) {
        bool matches = (password != NULL) && (passwordLength == requiredLength);
        if (matches) {
            for (size_t i = 0; i < requiredLength; ++i) {
                if (password[i] != DCC_PASSWORD[i]) {
                    matches = false;
                    break;
                }
            }
        }
        if (!matches) {
            printf("DeviceCommunicationControl: REJECTED (password failure)\n");
            *errorCode = ERROR_CODE_PASSWORD_FAILURE;
            return false;
        }
    }

    const char* action = (enableDisable == DCC_ENABLE) ? "enable (resume communication)" :
                         (enableDisable == DCC_DISABLE) ? "disable (1) - DEPRECATED, the stack will reject this" :
                         (enableDisable == DCC_DISABLE_INITIATION) ? "disable-initiation (keep responding)" :
                         "unknown";
    if (useTimeDuration) {
        printf("DeviceCommunicationControl: %s for %u minute(s)\n", action, timeDuration);
    } else {
        printf("DeviceCommunicationControl: %s (indefinitely)\n", action);
    }
    return true;
}

// AcknowledgeAlarm (AE-ACK-B). An operator acknowledges the Lift's Passenger_Alarm
// alarm. The stack tracks the acknowledged state; this callback lets the
// application react (and accept or reject). See B-AAC's main.cpp for why the
// trailing parameters are left unnamed.
bool AcknowledgeAlarm(const uint32_t deviceInstance, const uint32_t /*acknowledgingProcessIdentifier*/,
                      const uint16_t eventObjectType, const uint32_t eventObjectInstance,
                      const uint16_t /*eventStateAcknowledged*/, const uint8_t /*eventTimeStampYear*/,
                      const uint8_t /*eventTimeStampMonth*/, const uint8_t /*eventTimeStampDay*/,
                      const uint8_t /*eventTimeStampWeekday*/, const uint8_t /*eventTimeStampHour*/,
                      const uint8_t /*eventTimeStampMinute*/, const uint8_t /*eventTimeStampSecond*/,
                      const uint8_t /*eventTimeStampHundrethSecond*/, const char* /*acknowledgementSource*/,
                      const uint32_t /*acknowledgementSourceLength*/, const uint8_t /*acknowledgementSourceEncoding*/,
                      const bool /*timeOfAcknowledgementIsTime*/, const bool /*timeOfAcknowledgementIsSequenceNumber*/,
                      const bool /*timeOfAcknowledgementIsDateTime*/, const uint8_t /*timeOfAcknowledgementYear*/,
                      const uint8_t /*timeOfAcknowledgementMonth*/, const uint8_t /*timeOfAcknowledgementDay*/,
                      const uint8_t /*timeOfAcknowledgementWeekday*/, const uint8_t /*timeOfAcknowledgementHour*/,
                      const uint8_t /*timeOfAcknowledgementMinute*/, const uint8_t /*timeOfAcknowledgementSecond*/,
                      const uint8_t /*timeOfAcknowledgementHundrethSecond*/,
                      const uint16_t /*timeOfAcknowledgementSequenceNumber*/, uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Only Lift 1 "Mauve"'s Passenger_Alarm has an alarm in this example. Reject
    // an ack for anything else.
    if (eventObjectType != OBJECT_TYPE_LIFT || eventObjectInstance != LIFT_INSTANCE) {
        printf("AcknowledgeAlarm: rejected for object (type %u, instance %u) - no such alarm\n",
               eventObjectType, eventObjectInstance);
        *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    printf("AcknowledgeAlarm: Lift 1 (Mauve) Passenger_Alarm acknowledged\n");
    return true; // accept the acknowledgement
}

// -----------------------------------------------------------------------------
// 2e. F-OUTPUTS "set" callbacks (canonical in B-SA-CPP, copied here verbatim) -
// DS-WP-B/DS-WPM-B, the other genuinely new capability versus B-EM.
//
// The stack calls these when a client sends WriteProperty to a commandable
// output's Present_Value. The value arrives already decoded into the matching
// data type, together with the priority (1..16) the client wrote at. We store it
// in the object's Priority_Array; the stack recomputes Present_Value from the
// array on the next read. A WriteProperty of NULL relinquishes a slot and
// arrives through SetPropertyNull instead.
// -----------------------------------------------------------------------------

// REAL write - Analog Output 1 (Chartreuse) Present_Value.
bool SetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const float value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, const uint8_t priority,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_ANALOG_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Analog Output %u (Chartreuse) <- %.2f @ priority %u\n",
               objectInstance, value, EffectivePriority(priority));
        return true;
    }
    // DM-OCD-B: a runtime-created Analog Value's Present_Value is a plain
    // writable value (not commandable - no Priority_Array), so any priority
    // simply overwrites it, matching this profile's Verify list ("...serves
    // its required properties correctly").
    if (objectType == OBJECT_TYPE_ANALOG_VALUE && propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        CreatedAnalogValue* av = FindCreatedAnalogValue(objectInstance);
        if (av != NULL) {
            av->presentValue = value;
            printf("WriteProperty: Analog Value %u (created via DM-OCD-B) <- %.2f\n", objectInstance, value);
            return true;
        }
    }
    return false;
}

// ENUMERATED write - Binary Output 1 (Fuchsia) Present_Value (0/1).
bool SetPropertyEnumerated(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           const uint32_t value, const bool useArrayIndex,
                           const uint32_t propertyArrayIndex, const uint8_t priority,
                           uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_BINARY_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        if (value > 1) {
            *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return false;
        }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Binary Output %u (Fuchsia) <- %s @ priority %u\n",
               objectInstance, value ? "active" : "inactive", EffectivePriority(priority));
        return true;
    }
    return false;
}

// UNSIGNED write - Multi-State Output 1 (Indigo) Present_Value (state 1..3).
bool SetPropertyUnsignedInteger(const uint32_t deviceInstance, const uint16_t objectType,
                                const uint32_t objectInstance, const uint32_t propertyIdentifier,
                                const uint32_t value, const bool useArrayIndex,
                                const uint32_t propertyArrayIndex, const uint8_t priority,
                                uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        if (value < 1 || value > MULTI_STATE_OUTPUT_NUMBER_OF_STATES) {
            *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return false;
        }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Multi-State Output %u (Indigo) <- state %u @ priority %u\n",
               objectInstance, value, EffectivePriority(priority));
        return true;
    }
    return false;
}

// BOOLEAN write - DM-BR-B: Ivory (File 1)'s Archive marker.
bool SetPropertyBool(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const bool value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, const uint8_t priority,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)priority;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || objectType != OBJECT_TYPE_FILE ||
        objectInstance != FILE_INSTANCE || propertyIdentifier != PROPERTY_IDENTIFIER_ARCHIVE) {
        return false;
    }
    g_fileArchive = value;
    printf("WriteProperty: File 1 (Ivory) Archive <- %s\n", value ? "true" : "false");
    return true;
}

// NULL write - relinquish a commandable output's Present_Value at a priority.
bool SetPropertyNull(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex,
                     const uint8_t priority, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        CommandRelinquish(c, priority);
        printf("WriteProperty: relinquished %s %u @ priority %u\n",
               objectType == OBJECT_TYPE_ANALOG_OUTPUT ? "Analog Output" :
               objectType == OBJECT_TYPE_BINARY_OUTPUT ? "Binary Output" :
               "Multi-State Output",
               objectInstance, EffectivePriority(priority));
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// 2f. F-TIMESYNC (canonical in B-LD-CPP, copied here verbatim) - TimeSynchronization
// (DM-TS-B). ONLY TimeSynchronization is registered below (SERVICE_TIME_SYNCHRONIZATION
// in main()) - NOT UTCTimeSynchronization; this profile allows DM-TS-B *or* DM-UTC-B,
// and this example does local time only (see B-LD's main.cpp for the wire-tested
// rationale). Local_Date / Local_Time are served from g_syncedDateTime, seeded from
// the host clock at start-up so a read before any sync still succeeds.
// -----------------------------------------------------------------------------
struct SyncedDateTime {
    bool valid = false;
    uint8_t yearMinus1900 = 0, month = 1, day = 1, weekday = 1;
    uint8_t hour = 0, minute = 0, second = 0, hundredthSecond = 0;
};
static SyncedDateTime g_syncedDateTime;

static void InitSyncedDateTimeFromHost() {
    const time_t now = time(NULL);
    struct tm parts;
#if defined(_WIN32)
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    g_syncedDateTime.yearMinus1900 = (uint8_t)parts.tm_year;
    g_syncedDateTime.month = (uint8_t)(parts.tm_mon + 1);
    g_syncedDateTime.day = (uint8_t)parts.tm_mday;
    // BACnet weekday is 1=Monday..7=Sunday; struct tm's tm_wday is 0=Sunday..6=Saturday.
    g_syncedDateTime.weekday = (uint8_t)(parts.tm_wday == 0 ? 7 : parts.tm_wday);
    g_syncedDateTime.hour = (uint8_t)parts.tm_hour;
    g_syncedDateTime.minute = (uint8_t)parts.tm_min;
    g_syncedDateTime.second = (uint8_t)parts.tm_sec;
    g_syncedDateTime.hundredthSecond = 0;
    g_syncedDateTime.valid = true;
}

bool SetSystemTime(const uint32_t deviceInstance, const uint8_t year, const uint8_t month,
                   const uint8_t day, const uint8_t weekday, const uint8_t hour,
                   const uint8_t minute, const uint8_t second, const uint8_t hundrethSeconds) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    g_syncedDateTime.yearMinus1900 = year;
    g_syncedDateTime.month = month;
    g_syncedDateTime.day = day;
    g_syncedDateTime.weekday = weekday;
    g_syncedDateTime.hour = hour;
    g_syncedDateTime.minute = minute;
    g_syncedDateTime.second = second;
    g_syncedDateTime.hundredthSecond = hundrethSeconds;
    g_syncedDateTime.valid = true;
    printf("TimeSynchronization: %u-%02u-%02u %02u:%02u:%02u (Local_Date/Local_Time now read this back)\n",
           (unsigned)(1900 + year), month, day, hour, minute, second);
    return true;
}

bool GetPropertyDate(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     uint8_t* yearMinus1900, uint8_t* month, uint8_t* day, uint8_t* weekday,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || objectType != OBJECT_TYPE_DEVICE ||
        objectInstance != g_deviceInstance ||
        propertyIdentifier != PROPERTY_IDENTIFIER_LOCAL_DATE || !g_syncedDateTime.valid) {
        return false;
    }
    *yearMinus1900 = g_syncedDateTime.yearMinus1900;
    *month = g_syncedDateTime.month;
    *day = g_syncedDateTime.day;
    *weekday = g_syncedDateTime.weekday;
    return true;
}

bool GetPropertyTime(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     uint8_t* hour, uint8_t* minute, uint8_t* second, uint8_t* hundredthSecond,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || objectType != OBJECT_TYPE_DEVICE ||
        objectInstance != g_deviceInstance ||
        propertyIdentifier != PROPERTY_IDENTIFIER_LOCAL_TIME || !g_syncedDateTime.valid) {
        return false;
    }
    *hour = g_syncedDateTime.hour;
    *minute = g_syncedDateTime.minute;
    *second = g_syncedDateTime.second;
    *hundredthSecond = g_syncedDateTime.hundredthSecond;
    return true;
}

// -----------------------------------------------------------------------------
// 2g. F-REINIT (canonical pattern shared by B-AAC/B-ASC/B-LSC, copied here
// verbatim) - ReinitializeDevice (DM-RD-B). A management station asks the
// device to restart. reinitializedState: 0 = COLDSTART, 1 = WARMSTART (2..6 are
// backup/restore states this example does not support). Do NOT restart here:
// returning true only tells the stack the request was accepted (the SimpleACK
// has not gone out on the wire yet) - record a deadline via
// CASExampleHelper::RequestRestart and let the main loop act once the ACK has
// had time to ship (CASExampleHelper::RestartDue).
// -----------------------------------------------------------------------------
bool ReinitializeDevice(const uint32_t deviceInstance, const uint32_t reinitializedState,
                        const char* password, const uint32_t passwordLength,
                        uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }
    if (!PasswordAccepted(password, passwordLength)) {
        printf("ReinitializeDevice: REJECTED (password failure)\n");
        *errorCode = ERROR_CODE_PASSWORD_FAILURE;
        return false;
    }
    if (reinitializedState == REINITIALIZE_STATE_COLDSTART) {
        printf("ReinitializeDevice: COLDSTART accepted (restarting in %u ms)\n",
               (unsigned)CASExampleHelper::RESTART_DELAY_MS);
        CASExampleHelper::RequestRestart(CASExampleHelper::RestartKind::Cold,
                                         CASExampleHelper::RESTART_DELAY_MS);
        return true;
    }
    if (reinitializedState == REINITIALIZE_STATE_WARMSTART) {
        printf("ReinitializeDevice: WARMSTART accepted (re-initializing in %u ms)\n",
               (unsigned)CASExampleHelper::RESTART_DELAY_MS);
        CASExampleHelper::RequestRestart(CASExampleHelper::RestartKind::Warm,
                                         CASExampleHelper::RESTART_DELAY_MS);
        return true;
    }
    printf("ReinitializeDevice: state %u not supported\n", reinitializedState);
    *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
    return false;
}

// -----------------------------------------------------------------------------
// DM-BR-B (F-BACKUP, NEW vs B-EC): File I/O (backup/restore payload) + the
// four backup/restore lifecycle callbacks. File 1 (Ivory) is a small
// in-memory STREAM-access file. Pattern copied byte-for-byte from B-ACC-CPP,
// canonical for this feature.
// -----------------------------------------------------------------------------
bool ReadFile(const uint32_t deviceInstance, const uint32_t fileInstance, const uint32_t fileStart,
             const uint32_t requestedCount, uint8_t* fileData, uint32_t* fileDataLength,
             const uint32_t maxFileDataLength, bool* endOfFile, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || fileInstance != FILE_INSTANCE) {
        return false;
    }
    if (fileStart >= g_fileDataLength) {
        *fileDataLength = 0;
        *endOfFile = true;
        return true;
    }
    uint32_t available = g_fileDataLength - fileStart;
    uint32_t toCopy = available;
    if (toCopy > requestedCount) toCopy = requestedCount;
    if (toCopy > maxFileDataLength) toCopy = maxFileDataLength;
    memcpy(fileData, g_fileData + fileStart, toCopy);
    *fileDataLength = toCopy;
    *endOfFile = (fileStart + toCopy >= g_fileDataLength);
    return true;
}

bool WriteFile(const uint32_t deviceInstance, const uint32_t fileInstance, const int32_t fileStart,
              const uint8_t* fileData, const uint32_t fileDataLength, int32_t* ackFileStart,
              uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || fileInstance != FILE_INSTANCE) {
        return false;
    }
    uint32_t start = (fileStart == -1) ? g_fileDataLength : (uint32_t)fileStart;
    if (start + fileDataLength > FILE_MAX_SIZE) {
        return false; // fileFull
    }
    memcpy(g_fileData + start, fileData, fileDataLength);
    if (start + fileDataLength > g_fileDataLength) {
        g_fileDataLength = start + fileDataLength;
    }
    *ackFileStart = (int32_t)start;
    printf("WriteFile: Ivory <- %u byte(s) at offset %d\n", fileDataLength, (int)start);
    return true;
}

bool PrepareBackup(const uint32_t deviceInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("PrepareBackup: staging Ivory's %u byte(s) for AtomicReadFile\n", g_fileDataLength);
    return true; // the demo payload (g_fileData) is already live; nothing more to stage
}
bool CompleteBackup(const uint32_t deviceInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("CompleteBackup: backup session ended\n");
    return true;
}
bool PrepareRestore(const uint32_t deviceInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("PrepareRestore: ready to accept AtomicWriteFile into Ivory\n");
    return true;
}
bool CompleteRestore(const uint32_t deviceInstance, const bool wasAborted) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("CompleteRestore: restore session ended (wasAborted=%s), Ivory now %u byte(s)\n",
           wasAborted ? "true" : "false", g_fileDataLength);
    return true;
}

// -----------------------------------------------------------------------------
// DM-OCD-B (CreateObject/DeleteObject) - the headline addition over B-EC, and
// this repository is CANONICAL for it (F-OCD). See g_createdAnalogValues and
// its helpers, declared near the top of this file next to the F-OUTPUTS
// Commandable helpers, for the host-side storage these callbacks manage.
// -----------------------------------------------------------------------------

// Called from inside Tick(), for the CreateObject service only (never for
// BACnetStack_AddObject). Returning false has exactly one meaning here: the
// stack answers Error(resources, noSpaceForObject) and does not create the
// object - there is no error-code out-parameter on this callback (see the
// doc comment), so every decline below is that same answer regardless of why.
bool CreateObject(const uint32_t deviceInstance, const uint16_t objectType, const uint32_t objectInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Defensive only: BACnetStack_SetObjectTypeCreatable(analog-value, true) is
    // the only type marked creatable below, so the stack itself is expected to
    // reject CreateObject for every other object type - Error(object,
    // objectCreationNotPermitted) - before this callback ever fires for one.
    if (objectType != OBJECT_TYPE_ANALOG_VALUE) {
        printf("CreateObject: REJECTED - object type %u is not creatable (only analog-value is)\n", objectType);
        return false;
    }
    if (FindCreatedAnalogValue(objectInstance) != NULL) {
        // The stack already checks for an existing instance before calling this
        // callback; reaching here would mean our own table disagrees with the
        // stack's object database, which should not happen.
        printf("CreateObject: REJECTED - Analog Value %u already tracked by this example\n", objectInstance);
        return false;
    }
    CreatedAnalogValue* slot = FindFreeAnalogValueSlot();
    if (slot == NULL) {
        printf("CreateObject: REJECTED - no space left for another Analog Value (max %u)\n",
               MAX_CREATED_ANALOG_VALUES);
        return false;
    }
    slot->inUse = true;
    slot->objectInstance = objectInstance;
    slot->presentValue = 0.0f; // Present_Value starts at 0.0 until a client writes it
    printf("CreateObject: DM-OCD-B - created Analog Value %u\n", objectInstance);
    return true;
}

// Fires for a DeleteObject request AND as the rollback if CreateObject accepted
// an object that the stack then failed to finish creating (e.g. an initial-value
// WriteProperty in the same request failed). The two sites read the return
// value differently (see the doc comment): on DeleteObject, false aborts the
// request with Abort(other) and nothing is removed; on the rollback path the
// return is only logged and the stack erases the object regardless. Either way
// this function's job is the same - release whatever CreateObject allocated.
bool DeleteObject(const uint32_t deviceInstance, const uint16_t objectType, const uint32_t objectInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType != OBJECT_TYPE_ANALOG_VALUE) {
        printf("DeleteObject: REJECTED - object type %u was never created by this example\n", objectType);
        return false;
    }
    CreatedAnalogValue* slot = FindCreatedAnalogValue(objectInstance);
    if (slot == NULL) {
        // The stack has already rejected an unknown object (Error(object,
        // unknownObject)) before calling us, so reaching here with nothing
        // tracked means our table and the stack's disagree - report it rather
        // than silently succeed.
        printf("DeleteObject: REJECTED - Analog Value %u is not tracked by this example\n", objectInstance);
        return false;
    }
    slot->inUse = false;
    printf("DeleteObject: DM-OCD-B - removed Analog Value %u\n", objectInstance);
    return true;
}

// Build the 6-octet BACnet/IP connection string for the LOCAL SUBNET BROADCAST:
// the directed broadcast address (IP | ~mask) followed by the UDP port in network
// byte order. (When the mask is 0.0.0.0 - the helper's fallback - this collapses to
// the limited broadcast 255.255.255.255.)
static void LocalBroadcastConnString(uint8_t out[6]) {
    out[0] = (uint8_t)(g_ipAddress[0] | ~g_ipSubnetMask[0]);
    out[1] = (uint8_t)(g_ipAddress[1] | ~g_ipSubnetMask[1]);
    out[2] = (uint8_t)(g_ipAddress[2] | ~g_ipSubnetMask[2]);
    out[3] = (uint8_t)(g_ipAddress[3] | ~g_ipSubnetMask[3]);
    out[4] = (uint8_t)(g_bacnetIpUdpPort >> 8);
    out[5] = (uint8_t)(g_bacnetIpUdpPort & 0xFF);
}

// -----------------------------------------------------------------------------
// 3. main()
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Show printf output immediately, even when stdout is piped to a file.
    setvbuf(stdout, NULL, _IONBF, 0);

    // --- Load the CAS BACnet Stack -------------------------------------------
    if (!LoadBACnetFunctions()) {
        fprintf(stderr, "Error: failed to load the CAS BACnet Stack: %s\n",
                CASBACnetStackAdapter_LastError());
        return 1;
    }

    // --- Command line + version --------------------------------------------
    if (CASExampleHelper::HandleHelpAndVersionArgs(argc, argv, APP_NAME, APP_VERSION)) {
        return 0;
    }
    const uint16_t port = CASExampleHelper::ParsePortArg(argc, argv, 47808);
    g_deviceInstance = CASExampleHelper::ParseDeviceIdArg(argc, argv, g_deviceInstance);
    CASExampleHelper::PrintVersion(APP_NAME, APP_VERSION);

    // --- Bind the BACnet/IP socket -----------------------------------------
    if (!CASExampleHelper::SetupUDP(port)) {
        return 1;
    }

    // Capture the BACnet/IP addressing the Network Port object will report.
    g_bacnetIpUdpPort = port;
    if (!CASExampleHelper::GetLocalIPv4(g_ipAddress, g_ipSubnetMask)) {
        printf("FYI: could not read a local IPv4 address; Network Port IP_Address "
               "will report 0.0.0.0.\n");
    }

    // F-TIMESYNC: seed Local_Date/Local_Time from the host clock so they read
    // back something valid even before any TimeSynchronization arrives.
    InitSyncedDateTimeFromHost();

    // --- Register callbacks -------------------------------------------------
    CASExampleHelper::SetNetworkPortInstance(NETWORK_PORT_INSTANCE);
    CASExampleHelper::RegisterCommonCallbacks();
    BACnetStack_RegisterCallbackGetPropertyReal(GetPropertyReal);
    BACnetStack_RegisterCallbackGetPropertyEnumerated(GetPropertyEnumerated);
    BACnetStack_RegisterCallbackGetPropertyUnsignedInteger(GetPropertyUnsignedInteger);
    BACnetStack_RegisterCallbackGetPropertyCharacterString(GetPropertyCharString);
    BACnetStack_RegisterCallbackGetPropertyBool(GetPropertyBool);
    BACnetStack_RegisterCallbackGetPropertyOctetString(GetPropertyOctetString);
    // F-TIMESYNC: Local_Date / Local_Time.
    BACnetStack_RegisterCallbackGetPropertyDate(GetPropertyDate);
    BACnetStack_RegisterCallbackGetPropertyTime(GetPropertyTime);
    // F-ELEVATOR: the list/sequence callbacks. NEW vs B-EM: the fifth, write-side
    // elevator callback IS now registered - see section 2c.
    BACnetStack_RegisterCallbackGetListOfEnumerations(GetListOfEnumerations);
    BACnetStack_RegisterCallbackGetListElevatorGroupLandingCallStatus(GetListElevatorGroupLandingCallStatus);
    BACnetStack_RegisterCallbackGetSequenceLiftAssignedLandingCall(GetSequenceLiftAssignedLandingCall);
    BACnetStack_RegisterCallbackGetSequenceLiftRegisteredCarCall(GetSequenceLiftRegisteredCarCall);
    BACnetStack_RegisterCallbackGetSequenceLiftLandingDoorStatus(GetSequenceLiftLandingDoorStatus);
    BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl(SetElevatorGroupLandingCallControl); // NEW vs B-EM
    // F-OUTPUTS: the "set" callbacks that make this a controller (DS-WP-B/DS-WPM-B).
    BACnetStack_RegisterCallbackSetPropertyReal(SetPropertyReal);
    BACnetStack_RegisterCallbackSetPropertyEnumerated(SetPropertyEnumerated);
    BACnetStack_RegisterCallbackSetPropertyUnsignedInteger(SetPropertyUnsignedInteger);
    BACnetStack_RegisterCallbackSetPropertyBool(SetPropertyBool);       // DM-BR-B (NEW vs B-EC): Ivory's Archive
    BACnetStack_RegisterCallbackSetPropertyNull(SetPropertyNull);
    // Device-management + alarming callbacks this profile needs.
    BACnetStack_RegisterCallbackDeviceCommunicationControl(DeviceCommunicationControl); // DM-DCC-B
    BACnetStack_RegisterCallbackAcknowledgeAlarm(AcknowledgeAlarm);                     // AE-ACK-B
    BACnetStack_RegisterCallbackReinitializeDevice(ReinitializeDevice);                 // DM-RD-B (NEW vs B-EM)
    BACnetStack_RegisterCallbackSetSystemTime(SetSystemTime);                          // DM-TS-B (NEW vs B-EM)
    // DM-OCD-B (NEW vs B-EC, canonical for F-OCD): CreateObject/DeleteObject.
    BACnetStack_RegisterCallbackCreateObject(CreateObject);
    BACnetStack_RegisterCallbackDeleteObject(DeleteObject);
    // DM-BR-B (NEW vs B-EC, F-BACKUP): the backup/restore payload carrier (File 1
    // "Ivory") plus the four backup/restore lifecycle callbacks.
    BACnetStack_RegisterCallbackReadFile(ReadFile);
    BACnetStack_RegisterCallbackWriteFile(WriteFile);
    BACnetStack_RegisterCallbackPrepareBackup(PrepareBackup);
    BACnetStack_RegisterCallbackCompleteBackup(CompleteBackup);
    BACnetStack_RegisterCallbackPrepareRestore(PrepareRestore);
    BACnetStack_RegisterCallbackCompleteRestore(CompleteRestore);

    // --- Create the device --------------------------------------------------
    if (!BACnetStack_AddDevice(g_deviceInstance)) {
        printf("Error: Failed to add the Device %u.\n", g_deviceInstance);
        return 1;
    }

    // Enable the services a B-EC must execute. We set each one explicitly so the
    // profile requirements are obvious. NEW vs B-EM: WriteProperty, WritePropertyMultiple,
    // ReinitializeDevice, TimeSynchronization.
    // Service numbers with no matching CASBACnetStackExampleConstants.h entry
    // in common/ - named locally, re-verified against BACnetServicesSupported.h
    // at the pin abd4cee1 rather than trusted from memory or the runbook table.
    static const uint32_t SERVICE_CREATE_OBJECT = 10;
    static const uint32_t SERVICE_DELETE_OBJECT = 11;
    static const uint32_t SERVICE_ATOMIC_READ_FILE = 6;
    static const uint32_t SERVICE_ATOMIC_WRITE_FILE = 7;
    const struct { uint32_t service; const char* name; } services[] = {
        { SERVICE_READ_PROPERTY,                 "ReadProperty (DS-RP-B)" },
        { SERVICE_READ_PROPERTY_MULTIPLE,        "ReadPropertyMultiple (DS-RPM-B)" },
        { SERVICE_WRITE_PROPERTY,                "WriteProperty (DS-WP-B)" },
        { SERVICE_WRITE_PROPERTY_MULTIPLE,       "WritePropertyMultiple (DS-WPM-B)" },
        { SERVICE_DEVICE_COMMUNICATION_CONTROL,  "DeviceCommunicationControl (DM-DCC-B)" },
        { SERVICE_REINITIALIZE_DEVICE,           "ReinitializeDevice (DM-RD-B)" },
        { SERVICE_TIME_SYNCHRONIZATION,          "TimeSynchronization (DM-TS-B)" },
        { SERVICE_ACKNOWLEDGE_ALARM,             "AcknowledgeAlarm (AE-ACK-B)" },
        { SERVICE_GET_EVENT_INFORMATION,         "GetEventInformation (AE-INFO-B)" },
        { SERVICE_CONFIRMED_EVENT_NOTIFICATION,  "ConfirmedEventNotification (AE-N-I-B)" },
        { SERVICE_UNCONFIRMED_EVENT_NOTIFICATION,"UnconfirmedEventNotification (AE-N-I-B)" },
        // NEW vs B-EC. Service numbers re-verified against BACnetServicesSupported.h
        // at the pin abd4cee1, not trusted from memory: createObject = 10,
        // deleteObject = 11, atomicReadFile = 6, atomicWriteFile = 7.
        { SERVICE_CREATE_OBJECT,                 "CreateObject (DM-OCD-B)" },
        { SERVICE_DELETE_OBJECT,                 "DeleteObject (DM-OCD-B)" },
        { SERVICE_ATOMIC_READ_FILE,              "AtomicReadFile (DM-BR-B)" },
        { SERVICE_ATOMIC_WRITE_FILE,             "AtomicWriteFile (DM-BR-B)" },
    };
    for (size_t i = 0; i < sizeof(services) / sizeof(services[0]); ++i) {
        if (!BACnetStack_SetServiceEnabled(g_deviceInstance, services[i].service, true)) {
            printf("Error: Failed to enable the %s service.\n", services[i].name);
            return 1;
        }
    }
    // DS-COV-B / DS-COVM-B: SubscribeCOV and SubscribeCOVPropertyMultiple. The
    // service enum has no separate SubscribeCOVPropertyMultiple constant in
    // CASBACnetStackExampleConstants.h (it mirrors only the handful the other
    // examples need), so it is named here with the verified bit number - see
    // BACnetServicesSupported.h: subscribeCovPropertyMultiple = 41.
    static const uint32_t SERVICE_SUBSCRIBE_COV = 5;
    static const uint32_t SERVICE_SUBSCRIBE_COV_PROPERTY_MULTIPLE = 41;
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_SUBSCRIBE_COV, true)) {
        printf("Error: Failed to enable the SubscribeCOV (DS-COV-B) service.\n");
        return 1;
    }
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_SUBSCRIBE_COV_PROPERTY_MULTIPLE, true)) {
        printf("Error: Failed to enable the SubscribeCOVPropertyMultiple (DS-COVM-B) service.\n");
        return 1;
    }

    // Discovery: Who-Is/I-Am (DM-DDB-B) and Who-Has/I-Have (DM-DOB-B) - answer.
    // NEW vs B-EM (DM-DDB-A): this profile also INITIATES discovery, demonstrated
    // on demand with the 'd' key (see the main loop below), not unconditionally
    // at start-up - enabling the service is what lets BACnetStack_SendWhoIs work.
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_IS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_AM, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_HAS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_HAVE, true)) {
        printf("Error: Failed to enable the discovery services (Who-Is/I-Am, Who-Has/I-Have).\n");
        return 1;
    }

    // --- Add the read-only base sensor objects (series convention) ----------
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT, ANALOG_INPUT_INSTANCE)) {
        printf("Error: Failed to add Analog Input %u (Bronze).\n", ANALOG_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_BINARY_INPUT, BINARY_INPUT_INSTANCE)) {
        printf("Error: Failed to add Binary Input %u (Emerald).\n", BINARY_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT, MULTI_STATE_INPUT_INSTANCE)) {
        printf("Error: Failed to add Multi-State Input %u (Hot Pink).\n", MULTI_STATE_INPUT_INSTANCE);
        return 1;
    }

    // --- Add the commandable OUTPUT objects (F-OUTPUTS, NEW vs B-EM) --------
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_OUTPUT, ANALOG_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Analog Output %u (Chartreuse).\n", ANALOG_OUTPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_BINARY_OUTPUT, BINARY_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Binary Output %u (Fuchsia).\n", BINARY_OUTPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_OUTPUT, MULTI_STATE_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Multi-State Output %u (Indigo).\n", MULTI_STATE_OUTPUT_INSTANCE);
        return 1;
    }

    // --- Add the Network Port object ----------------------------------------
    if (!BACnetStack_AddNetworkPortObject(
            g_deviceInstance, NETWORK_PORT_INSTANCE,
            NETWORK_PORT_NETWORK_TYPE_IPV4,
            NETWORK_PORT_PROTOCOL_LEVEL_BACNET_APPLICATION,
            0,  // networkNumber: not configured
            NETWORK_NUMBER_QUALITY_UNKNOWN,
            NETWORK_PORT_REFERENCE_PORT_NONE)) {
        printf("Error: Failed to add Network Port 1 (Vermilion).\n");
        return 1;
    }

    // --- AE-EL-I-B: Event Log 1 "Beige" (NEW vs B-EC, canonical F-EVENTLOG) --
    // See the constant block above: every property here is stack-held, not
    // served through this file's callbacks.
    if (!BACnetStack_AddEventLogObject(g_deviceInstance, EVENT_LOG_INSTANCE, EVENT_LOG_MAX_BUFFER_SIZE)) {
        printf("Error: Failed to add Event Log 1 (Beige).\n");
        return 1;
    }
    // Log_Enable is one of the stack-held properties BACnetStack_AddEventLogObject
    // already enables and defaults to writable - explicit here for clarity/review,
    // matching this file's "every profile requirement is visible" style.
    if (!BACnetStack_SetPropertyWritable(g_deviceInstance, OBJECT_TYPE_EVENT_LOG,
                                         EVENT_LOG_INSTANCE, PROPERTY_IDENTIFIER_ENABLE, true)) {
        printf("Error: Failed to make Event Log 1 (Beige) Log_Enable writable.\n");
        return 1;
    }

    // --- SCHED-I-B: Schedule 1 "Saffron" + Calendar 1 "Cream" (NEW vs B-EC, --
    // pattern copied byte-for-byte from B-AAC-CPP/B-ALSC-CPP, canonical
    // F-SCHED-I). Saffron drives Analog Output 1 "Chartreuse" at
    // SCHEDULE_WRITE_PRIORITY.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_SCHEDULE, SCHEDULE_INSTANCE)) {
        printf("Error: Failed to add Schedule 1 (Saffron).\n");
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_CALENDAR, CALENDAR_INSTANCE)) {
        printf("Error: Failed to add Calendar 1 (Cream).\n");
        return 1;
    }
    if (!BACnetStack_AddScheduleObject(g_deviceInstance, SCHEDULE_INSTANCE)) {
        printf("Error: Failed to create the stack-held schedule data for Saffron.\n");
        return 1;
    }
    if (!BACnetStack_AddScheduleObjectPropertyReference(
            g_deviceInstance, SCHEDULE_INSTANCE,
            g_deviceInstance, OBJECT_TYPE_ANALOG_OUTPUT, ANALOG_OUTPUT_INSTANCE,
            PROPERTY_IDENTIFIER_PRESENT_VALUE, false, 0)) {
        printf("Error: Failed to point Saffron at Analog Output 1 (Chartreuse).\n");
        return 1;
    }
    if (!BACnetStack_SetSchedulePriorityForWriting(g_deviceInstance, SCHEDULE_INSTANCE, SCHEDULE_WRITE_PRIORITY)) {
        printf("Error: Failed to set Saffron's Priority_For_Writing.\n");
        return 1;
    }
    if (!BACnetStack_SetScheduleDefault(g_deviceInstance, SCHEDULE_INSTANCE,
                                        4 /*Real*/, 0, SCHEDULE_DEFAULT_VALUE)) {
        printf("Error: Failed to set Saffron's Schedule_Default.\n");
        return 1;
    }
    // Effective for the current calendar year - a wide, obviously-safe window
    // for a demo device; a real deployment would set the actual commissioning
    // period.
    if (!BACnetStack_SetScheduleEffectivePeriod(g_deviceInstance, SCHEDULE_INSTANCE,
                                                0, 1, 1, 0, 12, 31)) {
        printf("Error: Failed to set Saffron's Effective_Period.\n");
        return 1;
    }
    // One weekly transition: every Monday at 08:00, Chartreuse moves to the
    // demo value. (Weekly_Schedule day order is 0=Monday..6=Sunday - see
    // BACnetStack_AddScheduleWeeklyTimeValue's doc comment - NOT C's tm_wday.)
    if (!BACnetStack_AddScheduleWeeklyTimeValue(g_deviceInstance, SCHEDULE_INSTANCE,
                                                0 /*Monday*/, 8, 0, 0, 0,
                                                4 /*Real*/, 0, SCHEDULE_DEMO_VALUE)) {
        printf("Error: Failed to add Saffron's weekly Monday 08:00 transition.\n");
        return 1;
    }
    // One exception: an inline calendar-date entry (periodType 0 = a single
    // date, here 2026-12-25) rather than a reference to Cream's Date_List -
    // Cream's Date_List cannot be populated through the customer API yet
    // (issue #963; see TODO.md), so a calendar-REFERENCE exception would be
    // stored but would never actually match. The inline form has no such
    // dependency.
    uint32_t exceptionIndex = 0;
    if (!BACnetStack_AddScheduleExceptionEventWithCalendarEntry(
            g_deviceInstance, SCHEDULE_INSTANCE, 0 /*periodType: calendar Date*/,
            2026, 12, 25, 255 /*wd1: any*/,
            0, 255, 255, 255 /*y2/m2/d2/wd2: unused for periodType 0*/,
            1 /*eventPriority: highest*/, &exceptionIndex)) {
        printf("Error: Failed to add Saffron's 2026-12-25 exception event.\n");
        return 1;
    }
    if (!BACnetStack_AddScheduleExceptionTimeValue(g_deviceInstance, SCHEDULE_INSTANCE,
                                                   exceptionIndex, 0, 0, 0, 0,
                                                   4 /*Real*/, 0, SCHEDULE_EXCEPTION_VALUE)) {
        printf("Error: Failed to add Saffron's 2026-12-25 exception time-value.\n");
        return 1;
    }

    // --- DM-BR-B: File 1 "Ivory" (NEW vs B-EC, canonical F-BACKUP) ----------
    if (!BACnetStack_AddFileObject(g_deviceInstance, FILE_INSTANCE, true /*isWritable*/,
                                   true /*isConfigurationFile*/, (uint8_t)FILE_ACCESS_METHOD_STREAM)) {
        printf("Error: Failed to add File 1 (Ivory).\n");
        return 1;
    }
    if (!BACnetStack_SetPropertyWritable(g_deviceInstance, OBJECT_TYPE_FILE, FILE_INSTANCE,
                                         PROPERTY_IDENTIFIER_ARCHIVE, true)) {
        printf("Error: Failed to make Archive writable on File 1 (Ivory).\n");
        return 1;
    }
    if (!BACnetStack_SetBackupAndRestoreEnabled(g_deviceInstance, 5 /*backupPreparationTime*/,
                                                5 /*restorePreparationTime*/, 5 /*restoreCompletionTime*/,
                                                120 /*backupFailureTimeout*/, true)) {
        printf("Error: could not enable Backup and Restore (DM-BR-B).\n");
        return 1;
    }

    // --- DM-OCD-B: Analog Value is the ONLY creatable object type (NEW vs ----
    // B-EC, canonical F-OCD). SetObjectTypeCreatable's own doc comment warns
    // creatable=true is one-directional (it also marks the type as supported
    // in Protocol_Object_Types_Supported; creatable=false would NOT un-mark
    // it) - not a concern here since this is set once at start-up and never
    // toggled off. SetPropertyByObjectTypeWritable is the per-object-TYPE
    // variant of SetPropertyWritable: it applies to the default template used
    // for objects created LATER at runtime (see its doc comment), which is
    // exactly what a client-created Analog Value needs for Present_Value to be
    // writable - there is no per-instance object to call SetPropertyWritable
    // on yet at start-up.
    if (!BACnetStack_SetObjectTypeCreatable(g_deviceInstance, OBJECT_TYPE_ANALOG_VALUE, true)) {
        printf("Error: could not mark Analog Value creatable (DM-OCD-B).\n");
        return 1;
    }
    if (!BACnetStack_SetPropertyByObjectTypeWritable(g_deviceInstance, OBJECT_TYPE_ANALOG_VALUE,
                                                      PROPERTY_IDENTIFIER_PRESENT_VALUE, true)) {
        printf("Error: could not make a created Analog Value's Present_Value writable.\n");
        return 1;
    }

    // --- Enable the OPTIONAL properties we choose to expose ------------------
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_DEVICE,
                                        g_deviceInstance, PROPERTY_IDENTIFIER_DESCRIPTION, true)) {
        printf("Error: Failed to enable Description on the Device object.\n");
        return 1;
    }
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT,
                                        MULTI_STATE_INPUT_INSTANCE, PROPERTY_IDENTIFIER_STATE_TEXT, true)) {
        printf("Error: Failed to enable State_Text on Multi-State Input 1 (Hot Pink).\n");
        return 1;
    }

    // --- F-OUTPUTS (NEW vs B-EM): make the three output objects commandable --
    // See B-SA-CPP's main.cpp for why these calls matter for Analog/Binary/
    // Multi-State Output (mostly a no-op there - the stack already defaults them
    // commandable) but become load-bearing the moment this pattern is copied to
    // an optionally-commandable type (an Analog/Binary/Multi-State VALUE).
    struct CommandableObject { uint16_t type; uint32_t instance; };
    const CommandableObject outputs[] = {
        { OBJECT_TYPE_ANALOG_OUTPUT,      ANALOG_OUTPUT_INSTANCE },
        { OBJECT_TYPE_BINARY_OUTPUT,      BINARY_OUTPUT_INSTANCE },
        { OBJECT_TYPE_MULTI_STATE_OUTPUT, MULTI_STATE_OUTPUT_INSTANCE },
    };
    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
        if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                            PROPERTY_IDENTIFIER_PRIORITY_ARRAY, true) ||
            !BACnetStack_SetPropertyEnabled(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                            PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT, true) ||
            !BACnetStack_SetPropertyWritable(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                             PROPERTY_IDENTIFIER_PRESENT_VALUE, true)) {
            printf("Error: Failed to make object type %u instance %u commandable.\n",
                   outputs[i].type, outputs[i].instance);
            return 1;
        }
    }

    // --- F-COVM: make Analog Input 1 (Bronze) subscribable on TWO properties -
    // A plain SubscribeCOV on a standard object type (Analog Input included)
    // already works without this; SetPropertySubscribable is what lets a
    // SubscribeCOVPropertyMultiple request name Status_Flags too, so one
    // multi-property notification carries both.
    if (!BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                             ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE, true) ||
        !BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                             ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_STATUS_FLAGS, true)) {
        printf("Error: Failed to make Analog Input 1 (Bronze) Present_Value/Status_Flags subscribable.\n");
        return 1;
    }
    // Table capacities. 0 = unbounded is the same default the stack already
    // enforces (28800 s max lifetime); called explicitly so the profile's
    // intent is visible in the code, matching the series convention.
    if (!BACnetStack_SetCOVSettings(g_deviceInstance, 0, 28800) ||
        !BACnetStack_SetCOVMultipleSettings(g_deviceInstance, 0)) {
        printf("Error: Failed to configure COV / COV-Multiple subscription settings.\n");
        return 1;
    }

    // --- F-ELEVATOR: Elevator Group 1 (Maroon), Lift 1 (Mauve), Escalator 1 (Mint)
    // Maroon groups Mauve (a group of Lifts); supportLandingCallStatus=true turns
    // on the (read-only, in this device) LandingCalls property.
    //
    // Positive Integer Value 1 (Turquoise) - the object Maroon's Machine_Room_ID
    // will name - MUST be added first. CASBACnetStackDLL.h's doc comment on
    // BACnetStack_AddElevatorGroupObject reads as though the stack creates it
    // automatically ("If the Positive Integer Value Object does not exist, this
    // function will create it"); verified against the running stack, it does
    // NOT - AddElevatorGroupObject fails outright with "Create the Positive
    // Integer Value object before adding the Elevator Group object" otherwise.
    // Trust the measured behaviour, not the comment.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_POSITIVE_INTEGER_VALUE, MACHINE_ROOM_ID_INSTANCE)) {
        printf("Error: Failed to add Positive Integer Value 1 (Turquoise).\n");
        return 1;
    }
    if (!BACnetStack_AddElevatorGroupObject(g_deviceInstance, ELEVATOR_GROUP_INSTANCE,
                                            MACHINE_ROOM_ID_INSTANCE, ELEVATOR_GROUP_ID,
                                            true /*isGroupOfLifts*/, true /*supportLandingCallStatus*/)) {
        printf("Error: Failed to add Elevator Group 1 (Maroon).\n");
        return 1;
    }
    if (!BACnetStack_AddLiftOrEscalatorObject(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                              ELEVATOR_GROUP_INSTANCE, ELEVATOR_GROUP_ID, LIFT_INSTALLATION_ID)) {
        printf("Error: Failed to add Lift 1 (Mauve).\n");
        return 1;
    }
    // Mint is an Escalator, not a Lift - it cannot belong to Maroon (a group of
    // Lifts), so it is added with the "no reference" sentinel.
    if (!BACnetStack_AddLiftOrEscalatorObject(g_deviceInstance, OBJECT_TYPE_ESCALATOR, ESCALATOR_INSTANCE,
                                              NETWORK_PORT_REFERENCE_PORT_NONE, ESCALATOR_GROUP_ID,
                                              ESCALATOR_INSTALLATION_ID)) {
        printf("Error: Failed to add Escalator 1 (Mint).\n");
        return 1;
    }

    // Optional Lift properties this example demonstrates (each exercises one of
    // the four F-ELEVATOR list/sequence callbacks registered above).
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                        PROPERTY_IDENTIFIER_ASSIGNED_LANDING_CALLS, true) ||
        !BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                        PROPERTY_IDENTIFIER_REGISTERED_CAR_CALL, true) ||
        !BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                        PROPERTY_IDENTIFIER_LANDING_DOOR_STATUS, true)) {
        printf("Error: Failed to enable Lift 1 (Mauve)'s optional properties.\n");
        return 1;
    }
    // Optional Escalator properties this example demonstrates.
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_ESCALATOR, ESCALATOR_INSTANCE,
                                        PROPERTY_IDENTIFIER_POWER_MODE, true) ||
        !BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_ESCALATOR, ESCALATOR_INSTANCE,
                                        PROPERTY_IDENTIFIER_ESCALATOR_MODE, true)) {
        printf("Error: Failed to enable Escalator 1 (Mint)'s optional properties.\n");
        return 1;
    }

    // --- AE-N-I-B / AE-ACK-B / AE-INFO-B: Notification Class 1 (Crimson) -----
    if (!BACnetStack_AddNotificationClassObject(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            NC_PRIORITY_TO_OFFNORMAL, NC_PRIORITY_TO_FAULT, NC_PRIORITY_TO_NORMAL,
            true /*toOffNormalAckRequired*/, false /*toFaultAck*/, true /*toNormalAck*/)) {
        printf("Error: Failed to add Notification Class 1 (Crimson).\n");
        return 1;
    }
    uint8_t recipientMac[6];
    if (RECIPIENT_USE_BROADCAST) {
        LocalBroadcastConnString(recipientMac);
    } else {
        memcpy(recipientMac, RECIPIENT_IP, 4);
        recipientMac[4] = (uint8_t)(g_bacnetIpUdpPort >> 8);
        recipientMac[5] = (uint8_t)(g_bacnetIpUdpPort & 0xFF);
    }
    const uint8_t validDaysAll = 0x7F;
    if (!BACnetStack_AddRecipientToNotificationClass(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            validDaysAll,
            0, 0, 0, 0,         // from 00:00:00.00
            23, 59, 59, 99,     // to   23:59:59.99
            RECIPIENT_PROCESS_IDENTIFIER,
            false,              // issue UNCONFIRMED notifications (works to a broadcast)
            true, true, true,   // notify on to-offnormal, to-fault, to-normal
            false, 0,           // NOT using the device choice
            true,               // use the ADDRESS choice
            0,                  // network number 0 = this local network
            recipientMac, sizeof(recipientMac))) {
        printf("Error: could not seed the Notification Class recipient (Crimson).\n");
        return 1;
    }
    // Note: unlike B-AAC (AE-CRL-B), Recipient_List is NOT made writable here -
    // AE-CRL-B is not a required BIBB for this profile.

    // Arm an intrinsic ChangeOfState (boolean) algorithm on Lift 1 (Mauve)'s
    // Passenger_Alarm: alarmValue=true means "Passenger_Alarm == true is the
    // OFFNORMAL condition". See the file header comment for how this is
    // simulated (a 30-second timer, never a write).
    if (!BACnetStack_SetAlarmsAndEventsForObjectEnabled(
            g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
            NOTIFICATION_CLASS_INSTANCE, NOTIFY_TYPE_ALARM,
            true /*enableToOffNormal*/, false /*enableToFault*/, true /*enableToNormal*/,
            true /*enableEventDetection*/,
            true /*enabled - 6.x dropped this argument's default, so pass it explicitly*/)) {
        printf("Error: could not enable alarms on Lift 1 (Mauve).\n");
        return 1;
    }
    if (!BACnetStack_SetIntrinsicChangeOfStateAlgorithmBool(
            g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
            true /*alarmValue: Passenger_Alarm == true is OFFNORMAL*/,
            0 /*timeDelay*/, false, 0, true /*enable*/)) {
        printf("Error: could not arm the ChangeOfState algorithm on Mauve's Passenger_Alarm.\n");
        return 1;
    }

    // Who-Is is answered automatically once WHO_IS/I_AM are enabled above. Every
    // device in the series announces itself once at start-up with an I-Am.
    CASExampleHelper::SendIAm(g_deviceInstance);

    printf("FYI: Device %u (\"%s\") ready. Vendor ID %u. Accepts WriteProperty to "
           "Chartreuse/Fuchsia/Indigo and Landing_Call_Control on Maroon. Press "
           "'d' to broadcast a demo Who-Is (DM-DDB-A), or 'h' for help.\n",
           g_deviceInstance, DEVICE_NAME, VENDOR_IDENTIFIER);

    // --- Run the stack ------------------------------------------------------
    time_t lastAlarmToggle = time(NULL);
    bool running = true;
    while (running) {
        BACnetStack_Tick();

        // --- AE-N-I-B demo: flip the simulated Passenger_Alarm every 30 s ----
        // No write, no key - a controller still just reports what the
        // (simulated) elevator hardware is doing here, on its own schedule.
        time_t now = time(NULL);
        if (now - lastAlarmToggle >= PASSENGER_ALARM_SIMULATION_PERIOD_SECONDS) {
            g_liftPassengerAlarm = !g_liftPassengerAlarm;
            lastAlarmToggle = now;
            BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                    PROPERTY_IDENTIFIER_PASSENGER_ALARM);
            printf("Simulated Passenger_Alarm on Lift 1 (Mauve): %s\n",
                   g_liftPassengerAlarm ? "ACTIVE" : "cleared");
        }

        // --- F-REINIT: deferred restart (DM-RD-B) ----------------------------
        // ReinitializeDevice only ARMED the restart; RestartDue() fires once the
        // SimpleACK has had time to reach the wire (CASExampleHelper::RESTART_DELAY_MS).
        // This example has no hardware to reset, so COLDSTART honestly restores
        // every commanded/simulated value to its power-on state and re-announces
        // with an I-Am; WARMSTART re-initializes communications but keeps the
        // commanded outputs a controls engineer set.
        CASExampleHelper::RestartKind restartKind;
        if (CASExampleHelper::RestartDue(&restartKind)) {
            if (restartKind == CASExampleHelper::RestartKind::Cold) {
                printf("Restart: COLDSTART - restoring power-on state.\n");
                g_analogInput1Value = 21.5f;
                const Commandable analogOutputAtPowerOn = { { false }, { 0 }, 20.0 };
                const Commandable binaryOutputAtPowerOn = { { false }, { 0 }, 0.0 };
                const Commandable multiStateOutputAtPowerOn = { { false }, { 0 }, 1.0 };
                g_analogOutput = analogOutputAtPowerOn;
                g_binaryOutput = binaryOutputAtPowerOn;
                g_multiStateOutput = multiStateOutputAtPowerOn;
                g_liftOutOfService = false;
                g_liftPassengerAlarm = false;
                g_escalatorOutOfService = false;
                g_escalatorPassengerAlarm = false;
                g_escalatorPowerMode = true;
                g_queuedLandingCall = QueuedLandingCall();
            } else {
                printf("Restart: WARMSTART - re-initializing, keeping commanded values.\n");
            }
            CASExampleHelper::SendIAm(g_deviceInstance);
            printf("Restart: complete. Device %u is back.\n", g_deviceInstance);
        }

        switch (CASExampleHelper::PollKey()) {
            case CASExampleHelper::KeyCommand::Help:
                CASExampleHelper::PrintHelp(APP_NAME, APP_VERSION);
                break;
            case CASExampleHelper::KeyCommand::Quit:
                running = false;
                break;
            case CASExampleHelper::KeyCommand::ArrowUp:
                g_analogInput1Value += 1.1f;
                BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                        ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE);
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::ArrowDown:
                g_analogInput1Value -= 1.1f;
                BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                        ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE);
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::DiscoverRemote: {
                // NEW vs B-EM (DM-DDB-A): broadcast a demo Who-Is on demand.
                uint8_t broadcastConn[6];
                LocalBroadcastConnString(broadcastConn);
                BACnetStack_SendWhoIs(broadcastConn, sizeof(broadcastConn), NETWORK_PORT_INSTANCE,
                                      true /*broadcast*/, 0, NULL, 0);
                printf("DM-DDB-A: sent a Who-Is to discover other devices on the subnet.\n");
                break;
            }
            case CASExampleHelper::KeyCommand::DemoAdvance: {
                // SCHED-I-B demo (claimed by B-AAC, docs/menu-keys.md; NEW vs B-EC):
                // add a Weekly_Schedule transition for RIGHT NOW (today, current
                // h:m:s) instead of waiting for the pre-seeded Monday 08:00 entry.
                // The schedule engine evaluates Weekly_Schedule against wall-clock
                // time on every tick, so once this entry exists it is immediately
                // the latest transition today and Chartreuse (Analog Output 1)
                // moves to SCHEDULE_DEMO_VALUE at SCHEDULE_WRITE_PRIORITY on the
                // next tick.
                time_t demoAdvanceNow = time(NULL);
                struct tm nowTm;
#if defined(_WIN32)
                localtime_s(&nowTm, &demoAdvanceNow);
#else
                localtime_r(&demoAdvanceNow, &nowTm);
#endif
                // BACnetDailySchedule day order is 0=Monday..6=Sunday; tm_wday is
                // 0=Sunday..6=Saturday, so shift it.
                const uint8_t dayOffset = (uint8_t)((nowTm.tm_wday + 6) % 7);
                if (BACnetStack_AddScheduleWeeklyTimeValue(
                        g_deviceInstance, SCHEDULE_INSTANCE, dayOffset,
                        (uint8_t)nowTm.tm_hour, (uint8_t)nowTm.tm_min, (uint8_t)nowTm.tm_sec, 0,
                        4 /*Real*/, 0, SCHEDULE_DEMO_VALUE)) {
                    printf("Schedule 1 (Saffron): added a Weekly_Schedule entry for right now "
                           "(%02u:%02u:%02u) -> Analog Output 1 (Chartreuse) moves to %.1f at "
                           "priority %u on the next tick.\n",
                           nowTm.tm_hour, nowTm.tm_min, nowTm.tm_sec,
                           SCHEDULE_DEMO_VALUE, SCHEDULE_WRITE_PRIORITY);
                } else {
                    printf("Error: could not add the demo Weekly_Schedule entry.\n");
                }
                break;
            }
            case CASExampleHelper::KeyCommand::None:
            default:
                break;
        }

#if defined(_WIN32)
        Sleep(1); // 1 ms - be a good citizen, don't spin the CPU
#else
        usleep(1000);
#endif
    }

    CASExampleHelper::RestoreInput();
    CASExampleHelper::ShutdownUDP();
    return 0;
}
