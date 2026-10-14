# Concepts, C++ Features & Glossary

This document explains every major C++ feature and real-time scheduling concept used in RTSched, with a "why it is used here" section for each one.

---

## Part A: C++ Language Features & Standard Library

---

### 1. `struct` vs `class`

- **What it is:** Both define a user-defined type with members. The only difference in C++ is default access: `struct` defaults to `public`, `class` defaults to `private`.
- **Used in RTSched:** `Task`, `Job`, `JobRecord`, `TaskSummary`, `JobComparator` are all `struct` because they are plain data aggregates — all members are public and there is no invariant to protect.
- **Why here:** Using `struct` for pure data and `class` for objects with internal state (Scheduler, Watchdog, DeadlineMonitor) is a standard C++ convention that signals intent clearly.

---

### 2. `enum class`

- **What it is:** A strongly-typed enumeration. Unlike a plain `enum`, an `enum class` does not implicitly convert to integers and its enumerators are scoped.
- **Used in RTSched:** `enum class Policy { RM, EDF }` in `policy.h`.
- **Why here:** Prevents accidental mixing of policy values with integers. You must write `Policy::RM`, not just `RM`. This catches bugs at compile time.

```cpp
// Plain enum — dangerous
enum Policy { RM, EDF };
int x = RM;  // compiles silently — accidental integer conversion

// enum class — safe
enum class Policy { RM, EDF };
int x = Policy::RM;  // compile error — caught immediately
```

---

### 3. `std::function<void()>`

- **What it is:** A type-erased callable wrapper. It can store any callable that matches a given signature: a lambda, a function pointer, a functor, or a `std::bind` expression.
- **Used in RTSched:** `Task::body` stores the task's workload. `Watchdog::Callback` stores the user's recovery handler.
- **Why here:** Without `std::function`, you would have to choose between function pointers (cannot capture local variables) or templates (less flexible). `std::function` allows users to pass lambdas with captures — very ergonomic.

---

### 4. `std::move` and move semantics

- **What it is:** `std::move` casts a value to an rvalue reference, enabling the move constructor or move assignment operator instead of a copy. For types like `std::string` and `std::vector`, moving transfers ownership of the internal buffer without copying it.
- **Used in RTSched:** `Scheduler` constructor moves the task vector with `tasks_(std::move(tasks))`. `Watchdog` constructor moves the callback with `callback_(std::move(recovery_callback))`.
- **Why here:** Avoids unnecessary deep copies of potentially large data structures. Especially important for `std::function`, which can hold large lambda captures.

---

### 5. `std::chrono::steady_clock`

- **What it is:** A monotonic clock that never goes backwards, even if the system clock is adjusted. Provides nanosecond precision. `steady_clock::now()` returns a `time_point`.
- **Used in RTSched:** All timing measurements: `base_time_`, `next_release_`, `start_time_`, `stop_time`.
- **Why here:** Real-time scheduling requires a clock that cannot jump backwards (e.g., due to NTP adjustments or DST changes). `steady_clock` is the only standard C++ clock guaranteed to be monotonic.

---

### 6. `std::thread`

- **What it is:** A class that represents an OS-level thread of execution. Calling `std::thread t(func, args...)` spawns a new thread that runs `func(args...)` concurrently.
- **Used in RTSched:**
  - `Scheduler` launches a `dispatcher_thread_` for the dispatch loop.
  - `Watchdog` launches a `thread_` for the monitor loop.
- **Why here:** Real-time schedulers always use at least two threads: one for the dispatch loop (the "kernel") and one for the watchdog (the "safety monitor"). Using OS threads allows true concurrency.

---

### 7. `std::atomic<bool>`

- **What it is:** A boolean variable whose read/write operations are guaranteed to be atomic — indivisible and immediately visible to all threads — without needing a mutex.
- **Used in RTSched:**
  - `Scheduler::running_` — tells the dispatcher thread to stop.
  - `Watchdog::running_`, `Watchdog::active_`, `Watchdog::callback_fired_` — control the watchdog's state.
- **Why here:** Using a plain `bool` across threads causes undefined behaviour (data race). `std::atomic<bool>` eliminates the race with minimal overhead — no mutex required for simple flag operations.

---

### 8. `std::mutex` and `std::lock_guard`

- **What it is:** `std::mutex` is a mutual exclusion lock. `std::lock_guard<std::mutex>` acquires the lock on construction and releases it automatically on destruction (RAII pattern).
- **Used in RTSched:**
  - `DeadlineMonitor::mtx_` protects `records_` from concurrent read/write.
  - `Watchdog::mtx_` protects `current_task_`, `start_time_`, `timeout_limit_ms_`.
- **Why here:** Any data accessed from multiple threads must be protected. `lock_guard` ensures the lock is always released even if an exception is thrown — unlike manual `unlock()` calls.

---

### 9. `mutable` keyword

- **What it is:** Allows a member to be modified even inside a `const` member function.
- **Used in RTSched:** `mutable std::mutex mtx_` in `DeadlineMonitor`.
- **Why here:** `get_records()` and `get_summaries()` are logically `const` (they do not change the monitor's observable state). But they must lock `mtx_` to be thread-safe, and locking modifies the mutex object. `mutable` tells the compiler this is intentional.

---

### 10. `std::priority_queue` with Custom Comparator

- **What it is:** A container adaptor that maintains elements in heap order. By default, it is a max-heap (largest element at top). You can provide a custom comparator to change the ordering.
- **Used in RTSched:** `std::priority_queue<Job, std::vector<Job>, JobComparator> ready_queue_`.
- **Why here:** A priority queue is the natural data structure for a scheduler's ready queue. The comparator implements RM (shortest period first) or EDF (earliest deadline first) ordering without duplicating the sorting logic.

---

### 11. `std::chrono::duration_cast`

- **What it is:** Converts between duration types (e.g., nanoseconds to milliseconds), rounding towards zero.
- **Used in RTSched:** `duration_cast<milliseconds>(finish_tp - base_time_).count()` — converts a time_point difference to integer milliseconds.
- **Why here:** `steady_clock::now()` returns a `time_point` in implementation-defined units. `duration_cast` extracts the millisecond count needed for the monitor records.

---

### 12. `std::this_thread::sleep_until`

- **What it is:** Suspends the calling thread until a specified `time_point` is reached, releasing the CPU to other threads.
- **Used in RTSched:** In `dispatcher_loop()` when the ready queue is empty: `std::this_thread::sleep_until(earliest_next_release)`.
- **Why here:** Without `sleep_until`, the dispatcher would busy-spin consuming 100% CPU while waiting for the next release. `sleep_until` is the correct, efficient alternative.

---

### 13. Lambda Functions (Closures)

- **What it is:** An anonymous function defined inline using `[capture](params) { body }` syntax. It "captures" variables from the surrounding scope.
- **Used in RTSched:**
  - In `main.cpp` Scenario 4: `auto recovery = [](const std::string& name) { ... };`
  - In `monitor.cpp`: `[&](const TaskSummary& s) { return s.task_name == rec.task_name; }`
  - In `tests.cpp`: multiple lambda recovery callbacks with captured `std::atomic<bool>`.
- **Why here:** Lambdas allow inline definition of short functions without requiring a named function. Capture-by-reference `[&]` allows the lambda to observe and modify local variables, which is essential for test callbacks that need to signal the test when they are fired.

---

### 14. `#pragma once`

- **What it is:** A non-standard but universally supported preprocessor directive that ensures a header file is included only once per compilation unit.
- **Used in RTSched:** All 6 header files.
- **Why here:** Prevents double-definition errors when a header is transitively included multiple times. The standard alternative is `#ifndef / #define / #endif` include guards, but `#pragma once` is shorter and less error-prone.

---

## Part B: Real-Time Systems Concepts

---

### 15. Period (T)

The fixed time interval between successive releases (activations) of a task.
- T1's period = 50ms means T1 must start a new job every 50ms.

---

### 16. Worst-Case Execution Time (WCET, C)

The maximum time a task can take to execute on the given hardware, regardless of input data.
- Must be determined by analysis or measurement.
- `spin_busy(wcet_ms)` simulates a task that always takes exactly `wcet_ms` to run.

---

### 17. Deadline (D)

The time by which a job must complete after its release.
- **Implicit deadline model:** D = T (used in RTSched by default).
- **Constrained deadline:** D ≤ T.
- **Arbitrary deadline:** D can be any positive value.

---

### 18. Processor Utilization (U)

```
U = sum_i (C_i / T_i)
```
The fraction of CPU time consumed by all tasks. U=1.0 means the CPU is fully loaded. U>1.0 means the task set is infeasible regardless of policy.

---

### 19. Schedulability Analysis

The process of mathematically proving that a task set can meet all deadlines under a given policy.
- For RM: check U ≤ n*(2^(1/n) − 1) (sufficient, not necessary).
- For EDF: check U ≤ 1.0 (necessary and sufficient).

---

### 20. Release Jitter

The delay between a task's ideal release time and its actual dispatch time.
- In RTSched: `jitter_ms = start_ms - release_ms`.
- Caused by: non-preemptive blocking (a lower-priority task is already running).
- Impact: even if no deadline is missed, large jitter indicates poor timeliness.

---

### 21. Response Time

The total time from a task's release to its completion: `response_time_ms = finish_ms - release_ms`.
- Must be ≤ deadline for the job to be schedulable.

---

### 22. Execution Overrun

When a task executes longer than its WCET: `overrun_ms = max(0, actual_execution - wcet_ms)`.
- In real systems, overruns indicate an unexpected condition, hardware fault, or under-estimated WCET.
- In RTSched, overruns are deliberately injected in Scenario 4.

---

### 23. Preemption vs Non-Preemption

- **Preemptive:** A higher-priority task can interrupt and evict a running lower-priority task at any moment.
- **Non-preemptive:** Once a task starts, it runs to completion. Other tasks (even higher priority) must wait.
- RTSched uses non-preemption. Consequence: maximum blocking time = WCET of the longest lower-priority task.

---

### 24. Hyperperiod

The Least Common Multiple (LCM) of all task periods. After one hyperperiod, the entire schedule repeats identically.
- Example: T1(50), T2(100), T3(200) → LCM = 200ms.
- In 1000ms there are exactly 5 hyperperiods.

---

### 25. 3-Color DFS (not in RTSched, but in MISRAAUTOSAR)

Used in the MISRAAUTOSAR companion project to detect cycles (recursion) in a call graph:
- White: not visited.
- Grey: currently being explored (on the DFS stack).
- Black: fully explored.
A cycle is detected when the DFS encounters a Grey node.

---

## Part C: Quick Reference Table

| Term | Definition |
|------|-----------|
| ECU | Electronic Control Unit — embedded computer in a car |
| RTOS | Real-Time Operating System |
| WCET | Worst-Case Execution Time |
| RM | Rate-Monotonic — fixed priority scheduling |
| EDF | Earliest Deadline First — dynamic priority scheduling |
| ISO 26262 | Automotive functional safety standard |
| ASIL | Automotive Safety Integrity Level (A–D) |
| Jitter | Deviation of actual start time from ideal start time |
| Overrun | Execution time exceeding WCET |
| Hyperperiod | LCM of all task periods |
| Admission control | Pre-execution check that U ≤ schedulability bound |
| Non-preemptive | Running job cannot be interrupted |
| Watchdog | Independent timer that detects and recovers from hung tasks |
| `std::atomic` | Thread-safe variable without mutex overhead |
| `mutable` | Allows modification inside const member functions |
| `std::function` | Type-erased callable wrapper |
| RAII | Resource Acquisition Is Initialization — resources released in destructor |
