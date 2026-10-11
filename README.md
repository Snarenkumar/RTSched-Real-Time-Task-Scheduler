# RTSched - Real-Time Task Scheduler

A lightweight, priority-driven user-space real-time task scheduler written in modern C++20.

```
RTSched - Real-Time Task Scheduler | C++20, Multithreading, std::chrono, CMake, GoogleTest
 - Developed a priority-driven user-space real-time task scheduler implementing Rate-Monotonic (RMS) and
   Earliest Deadline First (EDF) policies with Liu & Layland schedulability bounds
   (U <= n(2^(1/n) - 1)) for pre-execution admission control.
 - Implemented a dedicated dispatcher thread with priority queue management, a deadline monitor tracking
   WCET, jitter and overruns, and a watchdog monitor thread with fault-injection recovery callbacks;
   exported Gantt execution timelines for timing verification.
```

---

## Architecture & Features

- **Pre-execution Admission Control**:
  - Computes processor utilization $U = \sum \frac{C_i}{T_i}$.
  - **Rate-Monotonic (RM)**: Evaluates the Liu & Layland bound $U \le n(2^{1/n} - 1)$.
  - **Earliest Deadline First (EDF)**: Evaluates dynamic optimal utilization bound $U \le 1.0$.
- **Dedicated Dispatcher Thread**:
  - Releases jobs periodically using `std::chrono::steady_clock`.
  - Maintains ready jobs in a `std::priority_queue`.
  - Dispatches jobs non-preemptively and uses `sleep_until` during idle periods.
- **Timing & Deadline Monitor**:
  - Records job start, finish, response time, deadline adherence, release jitter, and execution overruns.
  - Aggregates per-task statistical summaries (jobs run, misses, average/maximum jitter, overruns).
- **Watchdog Monitor Thread**:
  - Runs on an independent thread watching active job execution duration.
  - Triggers a user recovery callback if a job exceeds safety limits (default: $3 \times \text{WCET}$).
- **Fault Injection Framework**:
  - Injects execution anomalies into selected job instances to verify overrun detection and watchdog recovery.
- **Gantt Chart & CSV Export**:
  - Generates ASCII Gantt timeline visualizations directly in the console (5 ms per character).
  - Exports execution intervals to `timeline.csv` for post-run verification.

---

## Building the Project

### Prerequisites
- C++20 compliant compiler (MSVC 2019+, GCC 10+, or Clang 11+)
- CMake 3.20 or newer
- Git / Internet connection (for FetchContent GoogleTest)

### Build Commands

```bash
# Configure build tree
cmake -B build

# Compile static library, CLI demo, and test suite
cmake --build build --config Release

# Run automated unit and integration tests
./build/Release/rtsched_tests.exe   # Windows
# or
ctest --test-dir build -C Release --output-on-failure
```

---

## Running Demo Scenarios

The CLI demo executable provides 4 predefined scenarios:

```bash
# 1. Rate-Monotonic Schedulable Set (T1: 50/10, T2: 100/20, T3: 200/40, U = 0.60)
./build/Release/rtsched_demo rm

# 2. Earliest Deadline First Schedulable Set
./build/Release/rtsched_demo edf

# 3. Admission Comparison (U = 0.85: RM rejects [bound ~0.78], EDF accepts [bound 1.0])
./build/Release/rtsched_demo reject

# 4. Fault Injection with Watchdog Recovery Callback
./build/Release/rtsched_demo fault
```

---

## Sample Output

### Scenario 1: Rate-Monotonic (RM)
```
=== Scenario 1: Rate-Monotonic (RM) Schedulable Set ===
[Admission] Policy: RM | U = 0.6000 | Bound = 0.7798 -> ACCEPTED

=================== TIMING MONITOR SUMMARY ===================
Task      Jobs      Misses    Avg Jitter(ms) Max Jitter(ms) Overruns  
--------------------------------------------------------------
T1        20        0         11.45          30             0         
T2        10        0         16.30          25             0         
T3        5         0         36.80          40             0         
==============================================================

Execution Gantt Chart (1 char = 5 ms):
T1      |##............##....###.........###.......##............##..###.......###...|
T2      |..####................#####.................####..............#####.........|
T3      |......########..................................########....................|

[Output] Exported execution timeline to timeline.csv
```

### Scenario 4: Fault Injection & Watchdog Recovery
```
=== Scenario 4: Fault Injection & Watchdog Recovery ===
[Admission] Policy: RM | U = 0.6000 | Bound = 0.7798 -> ACCEPTED
RECOVERY: task T1 exceeded limit

=================== TIMING MONITOR SUMMARY ===================
Task      Jobs      Misses    Avg Jitter(ms) Max Jitter(ms) Overruns  
--------------------------------------------------------------
T1        20        1         12.85          32             1         
T2        10        0         23.00          64             1         
T3        5         0         38.20          42             1         
==============================================================

Execution Gantt Chart (1 char = 5 ms):
T1      |##............###...#############.........###...........###.###...|
T2      |..####..........................#####.......#####.............#####|
T3      |......#########.................................#########..........|

[Output] Exported execution timeline to timeline.csv
```

---

## Design Limitations & Notes

1. **Non-Preemptive Scheduling**: Once a job is dispatched, it runs to completion without preemption. While this eliminates context switching overhead, low-priority long-running jobs can cause release jitter for higher-priority tasks.
2. **Simulated Task Workloads**: Task bodies simulate CPU computation using busy-wait loops (`spin_busy`).
3. **Single Core Dispatching**: Jobs execute sequentially on a single dedicated dispatcher thread.
4. **Sufficient vs. Necessary Bounds**: The Liu & Layland RM bound ($U \le n(2^{1/n}-1)$) is *sufficient but not necessary*. A task set with $U > \text{bound}$ may still be schedulable under RM. In contrast, EDF's bound $U \le 1.0$ is both necessary and sufficient for periodic tasks with deadlines equal to periods.
5. **Windows Sleep Timing Granularity**: Standard Windows timers exhibit sleep resolution of approximately 1 to 15.6 ms. This introduces physical jitter in user-space execution, which the `DeadlineMonitor` transparently records and reports.
