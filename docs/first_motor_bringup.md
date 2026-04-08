# GRETA OS - First Motor Bring-Up Procedure

**Project:** GRETA OS - Companion Robotics Platform  
**Author:** Shrivardhan Jadhav  
**Year:** 2026  
**Status:** Hardware integration procedure

---

## 1. Purpose

This procedure defines the minimum safe process for first motor bring-up using the current GRETA OS control path:

```text
Command Processor -> Behavior Manager -> Task Manager -> Motion Link -> Arduino Motion Layer
```

The goal is to verify that:

- the rover remains stopped on boot
- `STOP` always overrides motion
- motion is only accepted in permitted modes
- safety faults force `SAFE`
- basic motor directions respond predictably under controlled conditions

This procedure is for controlled bench testing only. It is not a field test.

---

## 2. Test Preconditions

Before applying motor power, confirm all of the following:

- latest intended firmware is flashed to the ESP32 and Arduino motion controller
- rover chassis is mechanically stable
- drive wheels are lifted off the ground or drivetrain is otherwise unloaded
- test area is clear of hands, loose wiring, tools, and obstacles
- operator has physical access to motor power disconnect
- operator has access to dashboard `STOP`
- battery voltage is within expected range
- motor driver wiring has been checked for polarity and secure terminals
- communication link to the dashboard is stable

Do not proceed if any precondition is not met.

---

## 3. Required Safety Precautions

- Keep one person assigned as the safety operator for the entire test.
- Use the lowest practical motor supply condition for initial testing.
- Keep the rover supported so unintended motion cannot translate into chassis movement.
- Do not place hands near wheels, gears, or driveline during powered testing.
- Do not test autonomous movement during first bring-up.
- Abort immediately on unexpected spin, delayed stop, repeated reconnects, or state inconsistency.

Recommended physical controls:

- inline motor power switch or removable motor power connector
- accessible main battery disconnect
- clear verbal callout for `POWER CUT` and `STOP`

---

## 4. Initial System State

Expected startup conditions:

- mode starts in `IDLE` or other non-motion state
- active task is `IDLE`
- no latched critical health fault is present
- no motion command is pending
- motors remain stopped until an explicit test command is issued

If the system boots directly into motion-permitting state without operator intent, stop testing and correct firmware configuration first.

---

## 5. Bring-Up Sequence

### Step 1: Power-Off Inspection

- confirm wheel clearance from the bench
- confirm motor driver enable state is safe at power-up
- confirm serial and control wiring is fully seated
- confirm no exposed conductors can short under vibration

### Step 2: Logic Power-Up

- power the control electronics without enabling motor power if possible
- wait for firmware boot completion
- verify dashboard connection
- verify reported mode, state, and health are stable

Expected result:

- system reaches a non-moving ready condition
- no unsolicited motion command is issued

### Step 3: Safety Path Verification

Before any motion test, issue one manual `STOP`.

Verify:

- `STOP` is accepted immediately
- no motor movement occurs
- system does not report active motion

Then verify one fault path, for example:

- disconnect dashboard/network briefly, or
- trigger the intended safety inhibit condition used in current bench testing

Expected result:

- system enters `SAFE`
- motion remains blocked while `SAFE` is active

Restore the link or clear the test fault before continuing.

### Step 4: Enable Motor Power

- apply motor power while wheels remain lifted
- wait 3 to 5 seconds with no command input

Expected result:

- motors remain stopped
- no twitching, pulsing, or drift is observed

If any wheel moves without a command, cut motor power immediately.

### Step 5: Manual Motion Pulse Test

Use only short manual commands.

Recommended sequence:

1. issue a short forward pulse
2. issue `STOP`
3. issue a short reverse pulse
4. issue `STOP`
5. issue a short left turn pulse
6. issue `STOP`
7. issue a short right turn pulse
8. issue `STOP`

Command duration guidance:

- start with pulses of less than 0.5 seconds
- allow full stop between pulses
- do not queue commands rapidly

Verify after each pulse:

- correct wheel direction
- prompt stop response
- no continued motion after `STOP`
- no unexpected mode or state transition

### Step 6: Mode Gating Check

Attempt one motion command from a non-motion-permitting mode.

Expected result:

- command is rejected or ignored
- motors do not move
- system remains in safe, consistent state

Then switch intentionally to the allowed manual mode and repeat a single short motion pulse.

### Step 7: Communication Loss Check

While motors are idle, interrupt the active control link used for manual operation.

Expected result:

- system transitions to `SAFE`
- new motion commands are blocked until recovery conditions are satisfied

Do not test link loss while the wheels are still accelerating. Perform this only from a stopped condition during first bring-up.

---

## 6. Abort Conditions

Cut motor power immediately if any of the following occurs:

- motion occurs without operator command
- `STOP` does not halt motion immediately enough for bench control
- mode shown to the operator does not match expected behavior
- health or safety status changes without clear cause
- communication link repeatedly drops during command testing
- wheels continue moving after command timeout or link loss
- any component overheats, smokes, or emits unusual sound

After an abort:

- remove motor power
- leave logic power on only if needed for logs
- record the exact last command, mode, and observed behavior
- do not resume until root cause is understood

---

## 7. Minimum Pass Criteria

First motor bring-up is considered successful only if all conditions are met:

- no motion occurs at boot
- `STOP` reliably halts commanded motion
- motion only occurs in the intended manual test mode
- safety or link fault causes `SAFE`
- `SAFE` blocks further motion
- each commanded direction matches expected wheel behavior
- no unexplained resets, watchdog trips, or communication instability occur

---

## 8. Test Record

Record the following for the session:

- firmware revision or commit reference
- battery voltage at start
- motor driver configuration used
- dashboard/control link used
- results of boot stop check
- results of each motion pulse
- results of `STOP` verification
- results of `SAFE` verification
- any anomalies, even if intermittent

---

## 9. Immediate Next Steps After Pass

If the first bring-up passes, the next safe progression is:

1. repeat the same test with wheels lightly loaded
2. verify longer manual motion intervals
3. verify repeated stop-response behavior
4. verify recovery from `SAFE`
5. only then begin constrained ground testing at low speed

Do not enable autonomous testing until manual stop, mode gating, and safety recovery behavior are consistently repeatable.
