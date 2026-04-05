# GRETA OS — Boot Sequence

**Target:** ESP32, Arduino framework  
**Entry point:** `setup()` in `main.cpp`  
**Boot time target:** < 500 ms to scheduler active (excluding WiFi association)

---

## 1. Boot Layer Overview

Boot is divided into five ordered layers. Each layer must complete successfully before the next begins. A failure in any layer halts the boot with a Serial error message.

```
┌─────────────────────────────────────────────────────────┐
│  LAYER A — PLATFORM INIT                                │
│  Serial  •  USB CDC settle  •  greta_core_init()        │
├─────────────────────────────────────────────────────────┤
│  LAYER B — FSM INIT                                     │
│  state_init()  •  state_set(CONNECTING)                 │
├─────────────────────────────────────────────────────────┤
│  LAYER C — HARDWARE MODULE INIT                         │
│  bluetooth_init()  •  command_init()  •  telemetry_init │
│  network_set_command_callback()  •  network_init()      │
├─────────────────────────────────────────────────────────┤
│  LAYER D — GRETA OS KERNEL INIT                         │
│  event_bus_init()  •  scheduler_init()                  │
│  health_manager_init()  •  system_monitor_init()        │
├─────────────────────────────────────────────────────────┤
│  LAYER E — TASK REGISTRATION                            │
│  health_register() × N  •  scheduler_add_task() × N    │
│  Boot diagnostics printed                               │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
              loop() — scheduler active
```

---

## 2. Step-by-Step Sequence

### Step 1 — Serial init
```c
Serial.begin(115200);
delay(100);
```
**Why:** USB CDC on ESP32 requires ~100 ms to enumerate with the host. This is the only permissible `delay()` call in the entire firmware. All subsequent waits must be non-blocking.

---

### Step 2 — Greta core init
```c
greta_core_init();
```
**Why:** Stamps the boot ID and boot timestamp. Every subsequent log line and health manager uptime calculation depends on this timestamp being set first. Must precede all other module inits.

**Outputs:** Boot ID (CRC32 of MAC + `millis()`), firmware version string, hardware profile string printed to Serial.

---

### Step 3 — Boot diagnostics
```c
// Printed immediately after greta_core_init()
ESP.getFreeHeap()      // baseline heap before any module allocates
ESP.getFlashChipSize()
ESP.getCpuFreqMHz()
ESP.getSdkVersion()
```
**Why:** Captures pre-allocation heap as a baseline. If heap at this point is lower than expected, a hardware or SDK issue exists before any Greta module is involved.

---

### Step 4 — State machine init
```c
state_init();
state_set(STATE_CONNECTING, "boot");
```
**Why:** The FSM must be in a valid, defined state before any module can read it. Hardware modules initialised in the next step may call `state_get()` during their init. Setting `STATE_CONNECTING` immediately ensures movement is gated off before any link is established.

Order constraint: **Must precede all hardware module inits.**

---

### Step 5 — Hardware module init
```c
bluetooth_init();
command_init();
telemetry_init();
network_set_command_callback(on_dashboard_command);
network_init();
```
**Why this order:**

| Step | Reason |
|---|---|
| `bluetooth_init()` | No soft dependencies; hardware peripheral start |
| `command_init()` | Reads FSM state for gate init; must follow `state_init()` |
| `telemetry_init()` | Configures outbound channel; network not yet started |
| `network_set_command_callback()` | Callback must be registered before `network_init()` fires WiFi |
| `network_init()` | Fires `WiFi.begin()` last; callback registered; FSM valid |

`network_init()` is last because `WiFi.begin()` is non-blocking but can trigger the command callback almost immediately. All other modules must be fully initialised before any callback fires.

---

### Step 6 — OS kernel init
```c
event_bus_init();
scheduler_init();
health_manager_init();
system_monitor_init();
```
**Why this order:**

| Step | Reason |
|---|---|
| `event_bus_init()` | No dependencies; other OS modules publish to bus |
| `scheduler_init()` | Needs `greta_core` (uptime); sets up task table |
| `health_manager_init()` | Needs scheduler task count for sizing; needs event bus for fault events |
| `system_monitor_init()` | Needs health manager (registers its own heartbeat); needs greta_core uptime |

OS kernel is initialised after hardware modules deliberately: hardware modules must not assume the scheduler or health manager exist during their own init.

---

### Step 7 — Health registration
```c
health_register("network");
health_register("bluetooth");
health_register("command");
health_register("telemetry");
health_register("system_monitor");
```
**Why:** All heartbeat slots must be registered before the scheduler starts running tasks. A task that kicks an unregistered name is a programming error, not a fault, and should be caught at boot.

---

### Step 8 — Task registration
```c
scheduler_add_task(network_update,         10);
scheduler_add_task(bluetooth_update,       10);
scheduler_add_task(command_update,         20);
scheduler_add_task(state_update,           50);
scheduler_add_task(telemetry_update,      100);
scheduler_add_task(system_monitor_update, 100);
```
**Why this order:** Registration order is execution priority order within a simultaneous-due tick. Network must run before command watchdogs. See `TASK_TIMING.md` for rationale.

**Validation:** `scheduler_task_count()` is printed to confirm the expected number of tasks registered.

---

### Step 9 — Boot complete
```c
Serial.println(F("[MAIN] Greta OS kernel boot complete — entering runtime loop"));
```
Control passes to `loop()`. The scheduler begins dispatching tasks.

---

## 3. Boot Diagram

```
power-on
    │
    ▼
Serial.begin(115200)
delay(100)  ← only permitted delay
    │
    ▼
greta_core_init()
print: boot ID, firmware version, hw profile
print: free heap, flash size, CPU freq, SDK version
    │
    ▼
state_init()
state_set(STATE_CONNECTING)
    │                           ← FSM valid from this point
    ▼
bluetooth_init()
command_init()
telemetry_init()
network_set_command_callback()
network_init()  ← WiFi.begin() fires here (non-blocking)
    │
    ▼
event_bus_init()
scheduler_init()
health_manager_init()
system_monitor_init()
    │
    ▼
health_register() × 5
    │
    ▼
scheduler_add_task() × 6
print: task count confirmed
    │
    ▼
print: "[MAIN] Boot complete"
    │
    ▼
loop() ─────────────────────────────────────────────────────►
    │
    ├─ panic check
    ├─ scheduler_run_due_tasks()
    ├─ bluetooth_ack_drain()
    └─ link_recovery_update()
         │
         └─ when WiFi ok + BT ok:
              state_set(STATE_READY)
```

---

## 4. Boot Failure Modes

| Failure | Symptom | Response |
|---|---|---|
| Serial not available | No output | Hardware fault; check USB |
| `greta_core_init()` fails | No boot ID printed | Check flash; SDK issue |
| `scheduler_add_task()` returns false (MAX_TASKS exceeded) | Error printed | Boot halts; reduce task count or increase `MAX_TASKS` |
| WiFi never connects | State remains `CONNECTING` | No `STATE_READY`; system continues monitoring; no panic (link loss is `STATE_SAFE`, not `STATE_PANIC`) |
| Free heap at boot below threshold | Warning printed | System continues; monitor will track degradation |
