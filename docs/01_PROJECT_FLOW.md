# Project Flow: How RTSched Works

Welcome to the project flow guide!
This document explains how RTSched takes a set of periodic real-time tasks, schedules them, monitors their timing, and gives you a complete execution report.

---

## 1. What This Project Is (In 5 Simple Lines)

1. This project is a **real-time task scheduler** named `RTSched`.
2. A **real-time task scheduler** is a system that runs periodic jobs in strict timing order, ensuring every task meets its deadline.
3. It supports two scheduling policies: **Rate-Monotonic (RM)** and **Earliest Deadline First (EDF)**, both used in automotive ECUs and embedded safety systems.
4. It is built entirely from scratch in modern **C++20** using only the standard library plus GoogleTest for testing — no external frameworks.
5. It prints timing summaries, an ASCII Gantt chart, and exports an execution timeline to `timeline.csv` for post-run analysis.

### The Problem It Solves

In embedded systems — such as anti-lock brakes, engine control units, or airbag controllers — multiple tasks must share a single CPU core.
Each task has a **period** (how often it must run), a **deadline** (when it must finish), and a **WCET** (worst-case execution time).
Without a principled scheduler, tasks can miss deadlines, causing safety hazards.
RTSched acts like a laboratory-scale replica of real RTOS kernels (FreeRTOS, VxWorks, AUTOSAR RTE), letting you explore scheduling theory with real, runnable C++ code.

---

## 2. The Complete Pipeline Diagram

```
+---------------------+
|   Task Definitions  |  (name, period_ms, wcet_ms, deadline_ms, body, fault)
+---------------------+
          |
          v
+---------------------+
|  Admission Control  |  (policy.cpp: compute U = sum(wcet/period), check against bound)
+---------------------+
          |
          v  ACCEPTED or REJECTED
+---------------------+
|     Scheduler       |  (scheduler.cpp: starts Watchdog + Dispatcher threads)
+---------------------+
          |
          v  Jobs released periodically via steady_clock
+---------------------+
|   Priority Queue    |  (JobComparator: shortest period first (RM) or earliest deadline first (EDF))
+---------------------+
          |
          v  Top job is dispatched non-preemptively
+---------------------+
|   Job Execution     |  (task body or spin_busy(), fault injection on job #3)
+---------------------+
          |
          v  Parallel monitoring
+---------------------+
|     Watchdog        |  (watchdog.cpp: fires callback if job exceeds 3xWCET)
+---------------------+
          |
          v
+---------------------+
|  DeadlineMonitor    |  (monitor.cpp: jitter, response time, overruns, misses per job)
+---------------------+
          |
          v
+---------------------+
|    Gantt + CSV      |  (gantt.cpp: ASCII Gantt chart, timeline.csv export)
+---------------------+
          |
          v
+---------------------+
|  Console / File     |  (timing summary table, Gantt chart, CSV file)
+---------------------+
```

---

## 3. Stage-by-Stage Breakdown

### Stage 1: Task Definitions
- **File:** `include/rtsched/task.h`
- **Input:** User-provided timing parameters in main.cpp or test code.
- **Output:** A `Task` struct per task held in a `std::vector<Task>`.
- **Why:** Every scheduler stage needs to know a task's period, WCET, deadline, and workload body.

```cpp
Task t1("T1", 50, 10);   // name, period_ms=50, wcet_ms=10, deadline defaults to period
Task t2("T2", 100, 20);  // T2: period=100ms, WCET=20ms
```

---

### Stage 2: Admission Control
- **Files:** `include/rtsched/policy.h`, `src/policy.cpp`
- **Input:** Vector of `Task` structs and chosen `Policy`.
- **Output:** `true` (ACCEPTED) or `false` (REJECTED).
- **Why:** Before running, verify the CPU is not overloaded. An unschedulable task set guarantees deadline misses.

**Rate-Monotonic (RM) — Liu & Layland bound:**
```
U = sum(wcet_i / period_i)
Bound = n * (2^(1/n) - 1)
Accept if U <= Bound
```

**EDF — optimal policy:**
```
Accept if U <= 1.0
```

Example: T1=50/10, T2=100/20, T3=200/40 → U=0.60, RM bound(3)=0.7798 → ACCEPTED.

---

### Stage 3: Dispatcher Thread
- **File:** `src/scheduler.cpp` — `dispatcher_loop()`
- **What it does:**
  1. Stamps `base_time_` as t=0 using `std::chrono::steady_clock`.
  2. Sets all `next_release_[i] = base_time_` (all tasks start simultaneously).
  3. Loops: releases due jobs → pops highest-priority job → executes non-preemptively.
  4. When queue is empty, sleeps until next scheduled release.
- **Why:** This is the scheduler heartbeat. Nothing dispatches without this loop.

---

### Stage 4: Priority Queue & Job Ordering
- **File:** `src/policy.cpp` — `JobComparator::operator()()`
- **Why:** `std::priority_queue` is a max-heap. The comparator inverts order so the "best" job comes out first.

| Policy | Priority Rule             | Tie-break             |
|--------|---------------------------|-----------------------|
| RM     | Shorter period first      | Earlier release time  |
| EDF    | Earlier absolute deadline | Earlier release time  |

---

### Stage 5: Job Execution
- **File:** `src/scheduler.cpp` — `execute_job()`
- **Steps:**
  1. Arms Watchdog (`notify_job_start`).
  2. Runs `task.body()` or `spin_busy(wcet_ms)`.
  3. If `task.fault==true` AND `job_id==3`: runs `spin_busy(wcet_ms * 5)` — injected overrun.
  4. Disarms Watchdog (`notify_job_finish`).
  5. Records timestamps to `DeadlineMonitor`.
- **Why:** Actual computation and all timing measurements happen here.

---

### Stage 6: Watchdog Monitor
- **Files:** `include/rtsched/watchdog.h`, `src/watchdog.cpp`
- **What it does:** A separate thread polls every 2ms. If elapsed ≥ `wcet_ms * 3` while job is still active, fires the recovery callback once.
- **Why:** Simulates the hardware watchdog timer used in every automotive ECU to detect runaway tasks.

---

### Stage 7: DeadlineMonitor
- **Files:** `include/rtsched/monitor.h`, `src/monitor.cpp`
- **Computed metrics per job:**
  - `jitter_ms` = start_ms − release_ms (lateness of actual start vs. ideal release)
  - `response_time_ms` = finish_ms − release_ms
  - `overrun_ms` = max(0, execution_time − wcet_ms)
  - `deadline_met` = finish_ms <= deadline_ms
- **Why:** These are the exact metrics RTOS vendors and ISO 26262 auditors check for certification.

---

### Stage 8: Gantt Chart & CSV Export
- **Files:** `include/rtsched/gantt.h`, `src/gantt.cpp`
- **ASCII Gantt:** `#` = executing, `.` = idle, 1 character = 5ms slot.
- **CSV:** `task,start_ms,end_ms` — importable into Excel or Python for further analysis.
- **Why:** Visual proof that tasks interleave correctly and run non-preemptively.

---

## 4. The Two Policies Explained

### Rate-Monotonic (RM)

- **Rule:** Fixed priorities. Shortest period = highest priority. Never changes at runtime.
- **Origin:** Liu & Layland (1973) — one of the most cited papers in computer science.
- **Bound convergence:** As n→∞, the RM bound → ln(2) ≈ 0.693.
- **Key property:** Sufficient but NOT necessary. U > bound does not guarantee failure.

| n  | RM Bound |
|----|----------|
| 1  | 1.000    |
| 2  | 0.828    |
| 3  | 0.780    |
| 5  | 0.743    |
| ∞  | 0.693    |

### Earliest Deadline First (EDF)

- **Rule:** Dynamic priorities. At each dispatch, run the job with the earliest absolute deadline.
- **Bound:** U ≤ 1.0 — both necessary AND sufficient for periodic tasks (deadlines = periods).
- **Key property:** Optimal among all preemptive algorithms. If any scheduler can do it, EDF can.

---

## 5. Fault Injection Framework

When `Task` is constructed with `fault=true`, job instance #3 of that task deliberately runs `spin_busy(wcet_ms * 5)` — a 5× overrun. This simulates:
- An unexpected software hang (infinite loop)
- A hardware interrupt causing extended execution
- A timing anomaly in an embedded controller

The Watchdog detects elapsed > 3×WCET, fires the callback (e.g., prints "RECOVERY: task T1 exceeded limit"), and the DeadlineMonitor records the overrun and any resulting deadline miss.

---

## 6. Key Project Numbers

| Metric                        | Value                        |
|-------------------------------|------------------------------|
| Header files                  | 6                            |
| Implementation files          | 6                            |
| GoogleTest test cases         | 24                           |
| Scheduling policies           | 2 (RM, EDF)                  |
| Demo scenarios                | 4                            |
| Total source lines (no build) | ~590                         |
| Scenario 1 utilization        | U=0.60, RM ACCEPTED (bound ~0.78) |
| Scenario 3 utilization        | U=0.85, RM REJECTED, EDF ACCEPTED |
