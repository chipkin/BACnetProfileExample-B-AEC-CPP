# Tutorial - extending and reviewing the B-AEC example

[README.md](README.md) says what this example *is*. This document is the *how*:
how to extend it into your own device, who serves which property, how to review
the result for conformance, and what goes wrong when you get it subtly right.

Read this once before you start changing `main.cpp`. The most expensive mistake
in this example is silent, and the section it lives in is
[Add or extend an object](#add-or-extend-an-object).

- [Extending the example](#extending-the-example)
- [What each object type needs you to serve](#what-each-object-type-needs-you-to-serve)
- [Who serves what: DM-OCD-B's CreateObject/DeleteObject cycle](#who-serves-what-dm-ocd-bs-createobjectdeleteobject-cycle)
- [Reviewing your device](#reviewing-your-device)
- [Troubleshooting](#troubleshooting)

## Extending the example

The example is intentionally linear so it's easy to follow and copy from.

**Change a sensor's or output's value or name** - edit the constants /
callbacks in `main.cpp` (e.g. the initial value of `g_analogInput1Value`, or
the `"Bronze"` / `"Chartreuse"` strings in `GetPropertyCharString`).

**Change the device identity before you ship** - vendor ID, vendor name, model
name, description, firmware revision, application software version and device
name are all in the `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of
`main.cpp`, with a per-field comment on each saying what to change it to. That
block is the authoritative checklist; it is in the source rather than here so
it cannot be skipped by someone who only reads the code. `DEVICE_NAME` carries
its own warning: `Object_Name` **must be unique across the BACnet
internetwork**. It is a compile-time constant in this tutorial, which is fine
for one instance - a real product must make it per-unit configurable (a serial
number, DIP switches, a config file, or a `--deviceName` argument), not
hard-coded.

### Add or extend an object

Read this whole recipe before starting - the last step is the one that is easy
to miss and the one BTL will fail you for.

> **Why skipping a callback branch is SILENT, not loud.** Most of the
> `GetProperty*` callbacks match on **both** object type *and* instance
> (`objectInstance == LIFT_INSTANCE`), so a new instance of a type falls
> through every one of them. The stack errors only for the few properties it
> refuses to invent - on this device that includes `Present_Value` for most
> objects, `Car_Position`/`Car_Moving_Direction`/`Car_Door_Status`/
> `Passenger_Alarm` on a Lift, `Operation_Direction`/`Passenger_Alarm` on an
> Escalator, and `File_Size` on the File object. For everything else it
> **silently substitutes a default**:
>
> | Property | If you forget to serve it | Loud? |
> |---|---|:--:|
> | `Present_Value` (most types) | Error (`read-access-denied`) | yes |
> | `Object_Name` | reads back as the string **`"undefined"`** | **no** |
> | `Units` | reads back as **`no-units` (95)** | **no** |
> | `Out_Of_Service` | served on type alone for the base sensors/outputs - works by accident | n/a |
>
> It is worse than "wrong value": the object's `Property_List` **still
> advertises** the property. So the object actively claims to have it, and then
> answers with a default. Nothing on the wire says you forgot anything - a
> half-added object looks **healthy**, not broken. This exact mistake happened
> during this repository's own implementation: Binary Output 1's
> `GetPropertyEnumerated` branch was written with the array-slot case and the
> `Polarity` case, but the `Relinquish_Default` case was omitted. The build was
> clean (zero warnings) and the *other two* commandable outputs (Analog,
> Multi-State) read back fine, masking the bug until a wire test against Binary
> Output specifically returned `read-access-denied` on **both**
> `Relinquish_Default` and `Present_Value` - the stack cannot resolve a
> commandable object's `Present_Value` from an all-null `Priority_Array`
> without a servable `Relinquish_Default`.
>
> So when you copy the F-OUTPUTS commandable pattern (from
> [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP)) to a
> fourth object, walk **every** `Get*` callback branch for the type you're
> copying from (`GetPropertyReal`/`GetPropertyEnumerated`/
> `GetPropertyUnsignedInteger`), property by property, not just by eye - and do
> the same for the F-ELEVATOR list/sequence callbacks if you add another Lift
> or Escalator.

Then re-run the README's Verify steps **against the new object**, not just the
existing one - read every required property and **diff it against a working
object of the same type**. Any property that comes back `"undefined"`,
`no-units`, or a datatype zero where the working object returns something real
is a step you missed. Because the failure is silent (see the table above),
this diff is the only thing that catches it.

## What each object type needs you to serve

The application must serve every REQUIRED property the stack does not
generate. It differs per type - this is the checklist, so you do not have to
infer it:

| Object type | You must serve | Plus |
|---|---|---|
| Analog Input | `Present_Value` (Real), `Object_Name`, `Units` | — |
| Binary Input | `Present_Value` (Enumerated), `Object_Name` | `Polarity` |
| Multi-State Input | `Present_Value` (Unsigned), `Object_Name` | `Number_Of_States` |
| Analog/Binary/Multi-State Output | `Object_Name`, `Units`/`Polarity`/`Number_Of_States`, `Priority_Array` slots, `Relinquish_Default` | commandable: `Present_Value` writable |
| Network Port | `Object_Name`, `Network_Type`, `Protocol_Level`, `Changes_Pending`, IP addressing (`GetPropertyOctetString`) | — |
| Elevator Group | `Object_Name` | `Landing_Call_Control` writable |
| Lift | `Object_Name`, `Car_Position`, `Car_Moving_Direction`, `Car_Door_Status`, `Passenger_Alarm`, `Out_Of_Service`, `Fault_Signals` | optional list/sequence properties |
| Escalator | `Object_Name`, `Operation_Direction`, `Passenger_Alarm`, `Out_Of_Service` | optional `Power_Mode`, `Escalator_Mode` |
| Positive Integer Value | `Present_Value`, `Object_Name` | — |
| Notification Class | `Object_Name` | Priority/Ack_Required/Recipient_List are stack-held |
| Event Log | `Object_Name` | everything else is stack-held by the Event Log engine |
| Schedule | `Object_Name`, `Reliability`, `Out_Of_Service` | everything else is stack-held by the Schedule engine |
| Calendar | `Object_Name`, `Present_Value` | `Date_List` cannot be populated - see below |
| File | `Object_Name`, `File_Type`, `File_Size`, `Modification_Date`, `Archive`, `Read_Only` | — |
| Analog Value *(created via DM-OCD-B)* | `Present_Value` | everything else accepted at the stack default |

## Who serves what: DM-OCD-B's CreateObject/DeleteObject cycle

The single most common question when reading this file is "who answers this
property?" DM-OCD-B is this repository's headline feature and its canonical
pattern for the series, so it's the object worth tracing end to end:

| Step | What happens | Who does it |
|---|---|---|
| `CreateObject(analog-value, 2)` arrives | `BACnetStack_RegisterCallbackCreateObject`'s callback fires **before** the object exists in the stack's own database - this file allocates a slot in its own `g_createdAnalogValues` table, which **is** the object's storage | **app** |
| The object then appears in `Object_List` | the stack adds the entry to its own device-wide object list once the callback returns success | **stack** |
| `ReadProperty(Present_Value)` | served from `g_createdAnalogValues`, never from a stack-side per-object record (there is none) | **app** |
| `ReadProperty(Object_Name)` | no callback branch serves a created Analog Value's name - the stack substitutes its generic non-Device default | **stack default, accepted** (reads `"undefined"`) |
| `ReadProperty(Units)` / `Out_Of_Service` / `Event_State` / `Status_Flags` | same - deliberately left at the generic stack default | **stack default, accepted** |
| `WriteProperty(Present_Value, 42.5)` | writable at the **object-TYPE** level (`BACnetStack_SetPropertyByObjectTypeWritable`), since there is no per-instance object to configure before a client creates one | **app** (accepts the write into the table) |
| `DeleteObject(analog-value, 2)` | the callback registered with `BACnetStack_RegisterCallbackDeleteObject` frees the table slot; the stack removes the entry from `Object_List` | **app** + **stack** |
| `ReadProperty` of the deleted instance | correctly answers `Error(object: unknown-object)` | **stack** |
| `CreateObject(binary-input, 5)` | Binary Input was never marked creatable, so this is rejected with `CreateObjectError` before it ever reaches this file's `CreateObject` callback | **stack** |

Every object, not just this one, is in [docs/PICS.md](docs/PICS.md).

## Reviewing your device

After you have changed anything, review it against the conformance statement
rather than against "it looked fine in the explorer":

1. Regenerate [docs/PICS.md](docs/PICS.md) after editing `docs/objects.json`
   (see [Keeping the PICS honest](#keeping-the-pics-honest) below). A ⚠ row is a
   required property nothing serves.
2. Read **every** property listed for **every** object with a BACnet client, and
   compare the value against the PICS. `"undefined"`, `no-units` and a datatype
   zero are the three shapes a missed callback takes.
3. Diff a new or extended object against a working object of the same type.
   Anything that differs and shouldn't is a callback that matched on instance.
4. Exercise DM-OCD-B end to end: CreateObject an Analog Value, WriteProperty
   its `Present_Value`, DeleteObject it, and confirm the subsequent
   ReadProperty answers `unknown-object`. Also confirm CreateObject of a
   non-creatable type (e.g. `binary-input`) is rejected.
5. Confirm the WriteProperty/relinquish/out-of-range behaviour of the three
   commandable outputs, and that a landing call WriteProperty to
   `Landing_Call_Control` reads back through
   `GetListElevatorGroupLandingCallStatus`.

### Keeping the PICS honest

`docs/PICS.md` is partly generated. `docs/objects.json` describes each object and
who serves which property; `tools/gen-objects-properties.py` regenerates the
object tables from it plus the stack's own `docs/property-profile-reference.md`
at the pinned commit:

```bash
python tools/gen-objects-properties.py BACnetProfileExample-B-AEC-CPP            # rewrite
python tools/gen-objects-properties.py BACnetProfileExample-B-AEC-CPP --check    # fail if stale
```

(That tool lives in the example-series repository, not in this one. If you only
have this repository, edit the generated block by hand and keep it matching the
callbacks in `main.cpp`.)

When you add an object or a property to `main.cpp`, update `docs/objects.json`
in the same change and regenerate. The `app` list is what the callbacks serve;
`accepted` is for a required property you deliberately leave to the stack's
default, and each one needs a justification. Anything required, not in `app` and
not in `accepted`, comes out as a ⚠ row - that is a defect, not a feature.

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| On start-up the app prints a wall of red `Error:` lines but the device works | **Expected - this is not your bug**, and on THIS device it is louder than most siblings. Two ordinary sources are shared with every example: (1) the device receives its **own** broadcast I-Am and logs a decode cascade; (2) a one-time *"UUID has not been set..."* BACnet/SC notice. On top of those, adding Event Log 1 ("Beige") reproduces a **known, independently confirmed stack defect** ([chipkin/cas-bacnet-stack#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)): a continuous, non-fatal `Failed to set the date` / `Failed to set the time` flood starting from the very first `BACnetStack_Tick()` - 200 occurrences of each line were observed in a 5-second smoke-test window in this repository's own verification. The device keeps answering Who-Is and ReadProperty correctly throughout it. **This is not the same finding as B-ACC-CPP's isolated "AddEventLogObject alone" bisection** - see `TODO.md` #1 for the full object-combination comparison across this repository, B-ACC-CPP and B-ALSC-CPP; the flood's high line volume has also been observed to intermittently break the Windows CI runner's Git-Bash log-polling step (`TODO.md` #1, "Additional side effect observed in CI"). |
| `ReadProperty(File,1,File_Size)` fails with `read-access-denied` | **Not reproduced in this repository.** B-ACC-CPP documented this as part of [#2046](https://github.com/chipkin/cas-bacnet-stack/issues/2046); this repository's own build reads `File_Size` back correctly (0, empty file) despite an otherwise-identical File/Backup-Restore setup. If you hit the denial here, it did not reproduce in this repository's own verification session, so treat it as new information for #2046 rather than an already-understood repeat - see `TODO.md` #2. |
| CMake error: *"CAS BACnet Stack adapter not found under: ..."* | Submodules not initialized. Run `git submodule update --init --recursive` (or pass `-D CAS_STACK_DIR=...`). |
| `CASBACnetStackDLL.h: No such file or directory` | Same - submodules not checked out. |
| Windows: *"No CMAKE_CXX_COMPILER could be found"* | Install Visual Studio with the "Desktop development with C++" workload, then re-run from a fresh terminal. |
| First build seems stuck for minutes | Normal - it's compiling ~600 stack files. Only the first build is slow. |
| App prints *"Failed to bind UDP port 47808"* | Another BACnet program is already using 47808. Stop it, or run with `--port <n>`. |
| Client sends Who-Is but sees no I-Am | Firewall is blocking UDP 47808, or the client and device are on different subnets (Who-Is is a broadcast). Allow the port; test on the same subnet first. |
| `CreateObject` of anything other than `analog-value` is rejected | Correct - Analog Value is the only object type this example marks creatable (DM-OCD-B, F-OCD). Not a bug. |
| A created Analog Value's `Object_Name` reads back `"undefined"` | Correct - deliberately left at the stack's generic default; see `docs/objects.json`'s Analog Value entry and the CreateObject/DeleteObject table above. Not a bug. |
| WriteProperty of `Landing_Call_Control` | This write path is registered and code-reviewed, but was **not** independently wire-verified with a live client in this repository's own session (`BACnetLandingCallStatus`'s write-side SEQUENCE-CHOICE encoding was not cleanly expressible through the available test-client library) - see `TODO.md` #4. Confirm it yourself if you touch this code. |
