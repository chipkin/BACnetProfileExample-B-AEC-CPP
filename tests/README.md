# B-AEC Wire Tests

Tests that drive the running advanced elevator controller over BACnet/IP from
outside it. They are written against the series' shared harness in
[`../wiretest/`](../wiretest/README.md) (vendored, like `common/`); everything
specific to this device is here. Python 3 standard library only - nothing to
`pip install`.

What they cover today is **discovery**: a client can find the device and read
every property it says it has. They do not yet prove the B-AEC Profile Function
(L.13.1): the car does not move, and writes, CreateObject/DeleteObject, the
Schedule, the Event Log, backup/restore, COV and alarming are still checked by
hand (README "Verify", TODO.md).

## Running

Build the example (README "Build"), then from the repository root:

```bash
python3 tests/run_wire_tests.py --exe build/BACnetExampleBAEC
```

On Windows, `--exe build\Release\BACnetExampleBAEC.exe`. It ends with
`ALL WIRE TESTS PASSED` (exit 0) or the scripts that failed. `--port` (default
47863) is the UDP port the test starts the device on. It takes a few seconds.
CI (`.github/workflows/release.yml`) runs it on Linux after the smoke test.

## The setup

```
test client (127.0.0.1, ephemeral UDP port)  - the Peer
    |  BACnet/IP, unicast
BACnetExampleBAEC  UDP --port                - Device 389014
```

The Peer is a Python BACnet client (`wiretest.Client`), never another example's
binary. It plays a client such as Chipkin BACnet Explorer.

## `discovery_test.py` - every listed property reads as a value

The same walk as B-SS-CPP's discovery test, with this example's object list and
B-ACC-CPP's one addition for log objects.

1. **Who-Is** (for 389014, unicast) -> the device's I-Am.
2. **Device `Object_List`** names exactly the seventeen objects `main.cpp`
   creates at start-up: the Device, Analog Input 1, Binary Input 1, Multi-state
   Input 1, Analog Output 1, Binary Output 1, Multi-state Output 1, Network Port
   1, Elevator Group 1, Lift 1, Escalator 1, Positive Integer Value 1,
   Notification Class 1, Event Log 1, Schedule 1, Calendar 1 and File 1. This is
   the control: a walk over nothing would pass.
3. For each object, **`Property_List`** reads, and a ReadProperty of every
   property it lists - plus `Object_Identifier`, `Object_Name`, `Object_Type`
   and `Property_List`, which cl. 12.1.1.4.1 leaves out of the list - returns a
   ComplexACK. An Error, Reject, Abort or no answer prints an `[UNREADABLE]` line
   and fails the check. 135-2024 cl. 12.1.1.4.1: Property_List holds "one
   property identifier for each property that exists within the object".
4. **One exception: Event Log 1's `Log_Buffer` (131)** must be *refused*.
   135-2024 cl. 12.27.13: "The log buffer is not network accessible except
   through the use of the ReadRange service", and ReadProperty's error table
   (cl. 15.5.1.3.1) gives PROPERTY / READ_ACCESS_DENIED for "The property is not
   accessible using this service". That Error passes; a value, or any other
   answer, fails. (B-ACC-CPP added this rule first.)
5. Network Port 1's `IP_DNS_Server` (406), `FD_BBMD_Address` (418) and
   `FD_Subscription_Lifetime` (419) are each listed and read as a value - the
   three reads Chipkin BACnet Explorer found failing on B-SS at stack 6.0.21
   (B-SS-CPP #20-#22); every example has the same stack Network Port.

The walk reads only what `Property_List` lists, so it cannot see a required
property the stack leaves out of the list - Event Log 1's `Total_Record_Count`
at this pin (TODO.md).

## What it found

Run 2026-10-15 at stack `6.x` @ `64b2cfac5`, with `main.cpp` as it was at
example 1.0.2, Windows STATIC build: 22/23.

```
[UNREADABLE] File 1 property 43: Error class=2 code=27
[UNREADABLE] File 1 property 71: Error class=2 code=27
[UNREADABLE] Event Log 1 property 131: Error class=2 code=27
[UNREADABLE] Elevator Group 1 property 470: Error class=2 code=27
[UNREADABLE] Elevator Group 1 property 471: Error class=2 code=27
```

Error class 2 code 27 is `property` / `read-access-denied`.

- **File 1 `File_Type` (43) and `Modification_Date` (71)**: required, no stack
  default; `CASBACnetStackDLL.h` says the application serves them
  (`GetPropertyCharacterString`, `GetPropertyDate`/`Time`), and this example
  did not. Fixed in 1.1.0.
- **Event Log 1 `Log_Buffer` (131)**: the correct answer (step 4 above) - the
  test rule was missing, not the example.
- **Elevator Group 1 `Landing_Calls` (470) and `Landing_Call_Control` (471)**:
  the same `GetListElevatorGroupLandingCallStatus` defects as B-EC-CPP 1.1.0 -
  an empty list answered with `true`, and no value for `Landing_Call_Control`
  before the first write. Fixed in 1.1.0.

After the fixes: 24/24, 236 reads, on Windows (STATIC, x3) and Linux (WSL
Ubuntu 22.04, SOURCE build, x2), no `[RETRY]`.
