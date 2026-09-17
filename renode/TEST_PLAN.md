# Mine Monitoring System — Renode test plan

Start with:

```
renode renode/mine_test.resc
```

Two ways to drive a case:

* **Free run** — `start`, then fire stimuli from the monitor and watch the
  Python telemetry UI (port 3456). Good for the live demo.
* **Deterministic step** — leave the machine paused and advance virtual time
  explicitly: `emulation RunFor "0.4"`, then `runMacro $outputs`. This is how
  you *prove* a deadline, because virtual time is exact and repeatable.

`runMacro $outputs` reads GPIOB ODR: `0x00` idle, `0x01` pump on,
`0x04` alarm on, `0x05` both.

Confirm peripheral names once after loading with `peripherals`.

---

## UC-1 — Cold start is fail-safe

**Requirement:** pump must not run until every sensor reading is valid.

```
runMacro $sensors_healthy
start
# after ~1 s
runMacro $outputs        # expect 0x00 — no pump, no alarm
```

`sharedSensorData` starts zeroed, so all three `*Valid` flags are false and
`PumpManager_IsEnvironmentSafe()` refuses. `pumpMethaneInhibit` also starts
`pdTRUE`. Both clear after the first good methane read (100 ms).

**Negative variant — do not feed the ADCs at all.** Airflow reads 0, which is
`<= AIRFLOW_LOW_THRESHOLD`, so the alarm latches at t≈150 ms and never clears.
Worth showing deliberately: it demonstrates the fail-safe polarity of the
airflow check.

---

## UC-2 — Normal pumping cycle (200 ms event deadline)

**Requirement:** high level starts the pump, it runs until the low level.

```
pause
runMacro $water_high_on
emulation RunFor "0.2"
runMacro $outputs        # expect 0x01 within the 200 ms deadline
runMacro $flow_on        # flow follows the pump (see note below)
emulation RunFor "2"
runMacro $water_high_off
runMacro $water_low_on
emulation RunFor "0.2"
runMacro $outputs        # expect 0x00
runMacro $flow_off
```

**Note:** the platform does not couple PB0 (pump) to PB1 (flow sensor) —
you assert flow by hand. That decoupling is exactly what makes UC-9/UC-10
testable, so keep it, but remember that forgetting `$flow_on` after the pump
starts will raise a pump-fault alarm 900 ms later.

The chain being timed is EXTI0 → `waterLevelSemaphore` → WaterLevelTask →
`pumpCommandQueue` → PumpManagerTask → PB0.

---

## UC-3 — Methane goes critical while pumping (400 ms deadline)

**The headline safety requirement.**

```
pause
runMacro $water_high_on
emulation RunFor "0.3"
runMacro $flow_on
emulation RunFor "1"
runMacro $outputs           # 0x01, pump running

runMacro $methane_critical
emulation RunFor "0.4"      # exactly the spec budget
runMacro $outputs           # expect bit0 clear: 0x04 (alarm on, pump off)
```

Tighten it to find the real margin: `RunFor "0.1"` five times in a row,
reading `$outputs` after each. The pump should drop on the first or second
step. The path is MethaneTask → `PumpManager_SetMethaneCritical()` directly —
it does **not** go through the queue or PumpManagerTask, which is why the
400 ms is comfortable. Say that explicitly in the Zadatak 3 write-up.

Then show it does not restart while methane is high:

```
runMacro $water_high_off
runMacro $water_high_on
emulation RunFor "0.5"
runMacro $outputs           # still no pump — inhibit holds
runMacro $methane_safe
emulation RunFor "0.5"
# pump still off: the inhibit clears, but nothing re-issues a HIGH event.
runMacro $water_high_off
runMacro $water_high_on
emulation RunFor "0.3"
runMacro $outputs           # 0x01 again
```

That middle step is a genuine finding, not a bug per se: clearing the inhibit
does not re-evaluate the water level. On real hardware the level detector
stays asserted and no new edge arrives, so the sump keeps filling until the
next transition. Either re-post the current level when the inhibit clears, or
document it as intended operator-attended behaviour.

---

## UC-4 — Single transient sensor error is tolerated

**Requirement:** alarm only after **2 consecutive** failed reads.

Uses the firmware's own fault injector over UART (telemetry UI, or
`{"cmd":"CRASH_SENSOR","sensor":"CO","count":1}`):

```
pause
emulation RunFor "1"
# send  {"cmd":"SIM_ON"}
# send  {"cmd":"CRASH_SENSOR","sensor":"CO","count":1}
emulation RunFor "1"
runMacro $outputs           # expect 0x00 — one bad read, no alarm
```

`CRASH_SENSOR` only takes effect while `SIM_ON` is set (`SensorFault_ShouldFail`
gates on `simModeEnabled`) — easy to trip over during a demo.

---

## UC-5 — Two consecutive errors raise the alarm

```
# send  {"cmd":"CRASH_SENSOR","sensor":"CO","count":2}
emulation RunFor "0.5"
runMacro $outputs           # expect 0x04 within 2 x 150 ms
```

Repeat with `"sensor":"METHANE"` — that one must *also* drop the pump, since
`MethaneTask` calls `PumpManager_SetMethaneCritical(pdTRUE)` on the error path.
Two 100 ms periods = 200 ms, inside the 400 ms budget with room for the
response time. This is the case the spec's "account for one erroneous reading"
sentence is really about, so demo it next to UC-3.

---

## UC-6 — CO and airflow hazards

```
runMacro $co_critical
emulation RunFor "0.3"
runMacro $outputs           # 0x04
runMacro $co_safe

runMacro $airflow_blocked
emulation RunFor "0.3"
runMacro $outputs           # 0x04
runMacro $airflow_ok
```

Note the alarm **latches** — it stays on after the hazard clears until the
operator acknowledges. That is deliberate (see `task_alarm_manager.h`), but
say so before the examiner asks why the LED stays lit.

---

## UC-7 — Pump fails to start (t_d = 900 ms, t_c = 300 ms)

**Requirement:** command the pump on, see no flow, alarm after N periods.

```
pause
runMacro $flow_off
runMacro $water_high_on
emulation RunFor "0.3"
runMacro $outputs           # 0x01 — commanded on
emulation RunFor "0.85"     # still inside the 900 ms settling window
runMacro $outputs           # expect no alarm bit yet
emulation RunFor "0.35"     # crosses t_d + t_c
runMacro $outputs           # expect 0x05 — pump commanded on + alarm
```

`PUMP_FLOW_N_PERIODS = 6` × 150 ms = 900 ms = t_d exactly, with the check on
activation N+1. AlarmManagerTask takes no mutex at all, so it cannot be blocked
and easily meets t_c = 300 ms — a good point to make in the blocking analysis.

---

## UC-8 — Pump fails to stop

```
pause
runMacro $water_high_on
emulation RunFor "0.3"
runMacro $flow_on
emulation RunFor "1.5"
runMacro $water_high_off
runMacro $water_low_on
emulation RunFor "0.3"
runMacro $outputs           # 0x00 — commanded off
# leave the flow switch pressed: the pump is stuck running
emulation RunFor "1.3"
runMacro $outputs           # expect 0x04 — alarm on stuck flow
```

---

## UC-9 — Operator console

Over the socket (telemetry UI buttons, or raw JSON lines):

* `{"cmd":"PUMP_TOGGLE"}` — manual start. Note it bypasses
  `PumpManager_IsEnvironmentSafe()` and only respects the methane inhibit.
  Demo it with CO critical: the pump *will* start. Defensible (methane is the
  explosion hazard the spec names) but call it out as a conscious decision.
* `{"cmd":"ALARM_ACK"}` — clears the latch. With a hazard still present the
  alarm comes straight back within one period, because the sensor tasks
  re-raise their cause every period, not only on the rising edge. Show ack
  working on a *cleared* hazard first, then show the sticky case.
* `{"cmd":"SIM_ON"}` + `{"cmd":"SET_WATER_RATE","rate":60}` — the internal
  water model takes over from the GPIO detectors and generates level events
  on its own. Good for an unattended long soak.

---

## UC-10 — Event burst / debounce

```
pause
runMacro $water_high_on
emulation RunFor "0.01"
runMacro $water_high_off
emulation RunFor "0.01"
runMacro $water_high_on
emulation RunFor "0.01"
runMacro $water_high_off
emulation RunFor "0.5"
runMacro $outputs
```

Exercises the 50 ms debounce window and the binary (count-1) semaphore.
The spec guarantees ≥ 5 s between real events, so bounce is the only burst
source — worth showing that it is handled rather than assumed away.

---

## UC-11 — Soak

```
# SIM_ON, SET_WATER_RATE 40, then let it run
start
```

Leave it for several minutes with the telemetry archiver recording. What you
are looking for: no `STACK OVERFLOW:` or `MALLOC FAILED` line on the UART
(both hooks are wired), monotonic level cycling, and no drift in the sensor
task periods.

---

## Measuring for Zadatak 3

* `sysbus LogPeripheralAccess sysbus.gpioPortB true` — timestamped log of every
  ODR write, i.e. the exact virtual instant the pump or alarm pin changes.
  Pair it with the stimulus time to get end-to-end response times.
* `emulation RunFor` + `$outputs` bisection gives the same number without log
  parsing and is easier to put in a table.
* For per-task WCET rather than end-to-end response time, the honest route is
  `DWT->CYCCNT` around each task body, reported over the telemetry line, rather
  than inferring from the emulator — Renode does not model pipeline or flash
  wait states, so its instruction timing is not a WCET source. Say that in the
  report; it is the kind of caveat that earns marks.
