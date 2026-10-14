# Real World Context & Why Real-Time Scheduling Matters

This document explains why real-time scheduling exists, how it is used in cars and embedded systems, how RTSched relates to real RTOS products, and why this project is valuable to learn.

---

## 1. What Makes a System "Real-Time"?

A **real-time system** is one where correctness depends not only on the logical result of a computation, but also on the **time** at which the result is produced.

Three categories:

| Category | Definition | Example |
|----------|-----------|---------|
| **Hard real-time** | A missed deadline is a system failure (catastrophic) | Airbag deployment: must trigger in <20ms |
| **Firm real-time** | A missed deadline is useless but not catastrophic | Video frame: dropped frame hurts quality |
| **Soft real-time** | Occasional misses are tolerable | Music streaming: buffer prevents glitches |

RTSched targets **hard real-time** correctness concepts (admission control, deadline monitoring, watchdog), though the actual timing precision depends on the OS scheduler beneath it.

---

## 2. Real-Time Software in a Modern Car

A modern automobile contains **70–150 Electronic Control Units (ECUs)** — small embedded computers.
Each ECU runs multiple periodic software tasks simultaneously on a single processor core.

### Examples of real-time tasks in a car

| ECU | Task Name | Period | WCET | Consequence of Miss |
|-----|-----------|--------|------|---------------------|
| Brake ECU | ABS_Control | 10ms | 3ms | Wheels lock — loss of steering |
| Engine ECU | FuelInjection | 5ms | 1ms | Engine misfire — power loss |
| Airbag ECU | ImpactDetection | 1ms | 0.2ms | Airbag fails to deploy |
| Steering ECU | EPS_Update | 20ms | 5ms | Steering becomes unresponsive |
| Dashboard | SpeedDisplay | 100ms | 10ms | Display lag — cosmetic only |

These task sets look exactly like RTSched's Task structs: name, period_ms, wcet_ms, deadline_ms.

---

## 3. What is an RTOS?

A **Real-Time Operating System (RTOS)** is a specialised OS designed to guarantee worst-case timing behaviour.

Unlike Windows or Linux (which are designed for throughput and fairness), an RTOS prioritises **determinism** — you must be able to prove that task X will always finish within Y milliseconds.

### Common RTOS Products

| RTOS | Used In |
|------|---------|
| **FreeRTOS** | Microcontrollers (Arduino, STM32), IoT devices |
| **VxWorks** | Boeing 787, Mars Rovers, F-35 fighter jet |
| **QNX** | Car infotainment (BMW, Ford), medical devices |
| **OSEK/VDX** | Classic automotive (the standard before AUTOSAR) |
| **AUTOSAR OS** | Modern automotive ECUs (BMW, Mercedes, VW) |
| **Zephyr RTOS** | IoT, wearables, open-source |

RTSched implements the core scheduling concepts behind all these systems: periodic task model, priority queues, admission control, deadline monitoring, and fault detection.

---

## 4. ISO 26262 and Functional Safety

**ISO 26262** is the international standard for functional safety of automotive electrical/electronic systems.

It defines **Automotive Safety Integrity Levels (ASIL)**:

| Level | Risk | Examples |
|-------|------|----------|
| ASIL A | Low risk | Horn, headlights |
| ASIL B | Moderate | Windscreen wipers, parking sensors |
| ASIL C | High | Adaptive cruise control |
| ASIL D | Highest | ABS, airbags, electric power steering |

For ASIL-C and ASIL-D systems, ISO 26262 requires:
- **Schedulability analysis** — prove tasks can meet deadlines (this is exactly what admission control does).
- **Timing monitoring** — measure actual vs. expected execution times (this is exactly what `DeadlineMonitor` does).
- **Watchdog timers** — independent hardware/software timers that detect hung tasks and trigger safe recovery (this is exactly what `Watchdog` does).

RTSched implements all three concepts in software.

---

## 5. How RTSched Relates to Real RTOS Concepts

| RTSched Feature | Real RTOS Equivalent |
|-----------------|---------------------|
| `Policy::RM` | OSEK/AUTOSAR static priority scheduling |
| `Policy::EDF` | AUTOSAR Adaptive Platform dynamic scheduling |
| `rm_bound()` | Pre-deployment schedulability analysis (e.g., SymTA/S, Cheddar) |
| `check_admission()` | RTOS task-set validation before system start |
| `Scheduler::dispatcher_loop()` | RTOS kernel task scheduler / dispatcher |
| `std::priority_queue` | RTOS ready queue (e.g., FreeRTOS pxReadyTasksLists) |
| `Watchdog` | Hardware WDT (Watchdog Timer) on MCUs like RP2040, STM32 |
| `DeadlineMonitor` | RTOS runtime timing analysis / profiler |
| `spin_busy()` | Simulates deterministic computation (like a CPU-bound control loop) |
| `fault injection` | Automotive HIL/SIL fault injection testing |
| `timeline.csv` | RTOS trace output (e.g., Tracealyzer, SEGGER SystemView) |

---

## 6. Why Rate-Monotonic is Still Dominant in Automotive

Even though EDF is theoretically optimal, **RM is used in most real automotive systems** for these reasons:

1. **Predictability:** Fixed priorities mean execution order never changes. Engineers can prove system behaviour mathematically and on paper — critical for safety certification.
2. **Simplicity:** RM requires no runtime calculation of deadlines. The scheduler just picks the task with the highest (fixed) priority. This is much simpler to implement and verify in hardware.
3. **Tool support:** Tools like SymTA/S, Cheddar, and AUTOSAR Timing Architects all support RM analysis extensively.
4. **Well-understood failure modes:** When RM fails, it fails predictably — lower-priority tasks miss first. Engineers can choose which tasks to deprioritise.

---

## 7. The Non-Preemptive Choice in RTSched

RTSched implements **non-preemptive scheduling**: once a job starts, it runs to completion without interruption.

Real RTOSes typically implement **preemptive scheduling**: a higher-priority task can interrupt and evict a lower-priority task mid-execution.

**Why non-preemptive for RTSched?**
- Simpler to implement correctly (no context switch mechanisms needed).
- Avoids race conditions in task body code.
- Easier to analyse: the only source of interference is tasks already executing.
- Still demonstrates all the key real-time concepts (utilization, jitter, overruns, deadlines).

**Trade-off (Design Limitation):** In non-preemptive systems, even the highest-priority task can be blocked by a lower-priority task that is already executing. This is called **blocking time** and it increases jitter for high-priority tasks.

---

## 8. Why This Project Is Valuable to Learn

1. **It implements textbook theory in running code.** You can read Liu & Layland's 1973 paper and immediately see the formulas in `rm_bound()`.
2. **It uses real C++20 concurrency.** `std::thread`, `std::atomic`, `std::mutex`, `std::priority_queue`, `std::chrono` — all real tools used in production embedded software.
3. **It mirrors real RTOS architecture.** The Scheduler owns a dispatcher thread and a watchdog thread — the same two-thread structure used in production RTOS designs.
4. **It is measurable.** Every run produces real timing numbers. You can see the effect of non-preemptive blocking in the jitter values.
5. **It is interview-ready.** Every design decision (why RM?, why non-preemptive?, why atomic?, why spin_busy?) has a clear, defensible answer rooted in real-time systems theory.
