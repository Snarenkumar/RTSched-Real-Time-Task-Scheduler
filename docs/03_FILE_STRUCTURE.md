# Project File Structure & Code Map

This document explains the organisation of every directory and file in the `RTSched` project, with a step-by-step reading order and time estimates.

---

## 1. Complete Project Folder Tree

```
RTSched/
|
|-- .gitignore                     # Ignores build/, *.exe, *.obj, *.lib, .vs/, timeline.csv
|-- CMakeLists.txt                 # Build config: C++20, MSVC/GCC flags, GoogleTest via FetchContent
|-- README.md                      # Project overview, architecture, build & run guide
|-- timeline.csv                   # Generated at runtime: task,start_ms,end_ms execution log
|
|-- docs/                          # Comprehensive documentation library
|   |-- 01_PROJECT_FLOW.md         # Architecture pipeline, both policies, fault injection
|   |-- 02_HOW_TO_RUN.md          # Windows setup, CLI commands, outputs, troubleshooting
|   |-- 03_FILE_STRUCTURE.md      # This file: folder tree, file details, reading order
|   |-- 04_LINE_BY_LINE_EXECUTION.md # Walkthrough of Scenario 1 step by step
|   |-- 05_REAL_WORLD_AND_WHY.md  # RTOS context, ISO 26262, automotive scheduling
|   |-- 06_CONCEPTS_AND_GLOSSARY.md # C++20 features, RT theory terms, glossary
|   `-- 07_INTERVIEW_QA.md        # 50+ interview questions and answers
|
|-- include/rtsched/               # Public C++ header files (interfaces & declarations)
|   |-- task.h                     # Task struct: name, period, wcet, deadline, body, fault
|   |-- policy.h                   # Policy enum, Job struct, JobComparator, admission functions
|   |-- scheduler.h                # Scheduler class: run_for(), get_monitor(), spin_busy()
|   |-- monitor.h                  # JobRecord, TaskSummary, DeadlineMonitor class
|   |-- watchdog.h                 # Watchdog class: start/stop, notify, callback
|   `-- gantt.h                    # export_timeline_csv(), format_text_gantt(), print_text_gantt()
|
|-- src/                           # C++ implementation files (.cpp)
|   |-- main.cpp                   # CLI demo: 4 scenarios, run_and_report() helper
|   |-- policy.cpp                 # JobComparator, calculate_utilization(), rm_bound(), check_admission()
|   |-- scheduler.cpp              # Scheduler ctor/dtor, run_for(), dispatcher_loop(), execute_job()
|   |-- monitor.cpp                # record_job(), get_records(), get_summaries(), print_summary()
|   |-- watchdog.cpp               # start(), stop(), notify_job_start/finish(), monitor_loop()
|   `-- gantt.cpp                  # export_timeline_csv(), format_text_gantt(), print_text_gantt()
|
`-- tests/
    `-- tests.cpp                  # 24 GoogleTest cases: Task, Admission, PriorityQueue, Monitor, Integration, Gantt
```

---

## 2. File-by-File Detailed Explanation

---

### Root Files

#### 1. `.gitignore`
- **What it does:** Tells Git which generated files and folders to NOT track.
- **Contents:** Ignores `build/` (compiler outputs), `*.exe`, `*.lib`, `*.obj`, `.vs/` (VS caches), `timeline.csv` (runtime output).
- **Who uses it:** Git, GitHub Actions.

---

#### 2. `CMakeLists.txt` (72 lines)
- **What it does:** The central build blueprint for the entire project.
- **Contents:**
  - Sets `CMAKE_CXX_STANDARD 20` strictly.
  - Runs `find_package(Threads REQUIRED)` for `std::thread` support.
  - Fetches GoogleTest v1.14.0 via `FetchContent_Declare / FetchContent_MakeAvailable`.
  - Creates `rtsched` static library from 5 implementation files.
  - Creates `rtsched_demo` executable from `src/main.cpp`.
  - Creates `rtsched_tests` executable from `tests/tests.cpp`.
  - Applies `/W4 /WX` (MSVC) or `-Wall -Wextra -Werror` (GCC/Clang) on all targets.
  - Registers tests with `add_test(NAME RTSchedTests COMMAND rtsched_tests)`.
- **Interactions:** Uses all files in `src/`, `include/`, and `tests/`.

---

#### 3. `README.md` (139 lines)
- **What it does:** The front door of the repository on GitHub.
- **Contents:** Quick project description, architecture features table, build commands, 4 scenario examples with output, design limitations.
- **Interactions:** Points users to `docs/` for deeper study.

---

#### 4. `timeline.csv` (generated at runtime)
- **What it does:** Records every job's execution interval as a CSV row.
- **Format:** `task,start_ms,end_ms` — one row per job instance.
- **Who creates it:** `gantt.cpp` — `export_timeline_csv()` called from `main.cpp`.
- **Who uses it:** Post-processing in Excel, Python/pandas, or Gantt visualisation tools.

---

### Header Files (`include/rtsched/`)

#### 5. `task.h` (21 lines)
- **What it does:** Declares the fundamental data structure of the scheduler — the Task.
- **Contents:**
  - `struct Task` with fields: `name` (string), `period_ms`, `deadline_ms`, `wcet_ms` (int64_t), `body` (std::function<void()>), `fault` (bool).
  - Constructor: if `deadline <= 0`, sets `deadline_ms = period_ms` (implicit deadline model).
- **Interactions:** Included by `policy.h`, `scheduler.h`, and all source files.
- **Key C++ feature:** `std::function<void()>` allows storing any callable (lambda, function pointer, functor) as the task workload.

---

#### 6. `policy.h` (35 lines)
- **What it does:** Declares scheduling policy types, job structures, and admission control functions.
- **Contents:**
  - `enum class Policy { RM, EDF }` — type-safe policy selection.
  - `struct Job` — one job instance in the ready queue: `task_index`, `job_id`, `release_time`, `absolute_deadline`, `period_ms`, `wcet_ms`.
  - `struct JobComparator` — implements `operator()(const Job&, const Job&)` for the priority queue.
  - `calculate_utilization()` — computes U = sum(wcet/period).
  - `rm_bound(n)` — computes Liu & Layland bound: n*(2^(1/n)-1).
  - `check_admission()` — evaluates admission and optionally prints result.
- **Interactions:** Included by `scheduler.h`, `policy.cpp`, `tests/tests.cpp`.

---

#### 7. `scheduler.h` (35 lines)
- **What it does:** Declares the main Scheduler class that owns all subsystems.
- **Contents:**
  - `class Scheduler` with: `run_for(duration_ms)`, `get_monitor()`, `get_tasks()`, `spin_busy(ms)` (static).
  - Private members: `tasks_`, `policy_`, `comparator_`, `ready_queue_` (priority_queue), `monitor_`, `watchdog_`, `dispatcher_thread_`, `running_` (atomic bool), `base_time_`, `next_release_`, `job_ids_`.
  - Private methods: `dispatcher_loop()`, `release_jobs()`, `execute_job()`.
- **Key C++ features:** `std::priority_queue` with custom comparator, `std::atomic<bool>` for thread-safe stop flag, `std::thread` for the dispatcher.

---

#### 8. `monitor.h` (45 lines)
- **What it does:** Declares the data collection layer for timing metrics.
- **Contents:**
  - `struct JobRecord` — 11 fields: task_name, job_id, release/start/finish/deadline_ms, wcet_ms, response_time_ms, deadline_met, jitter_ms, overrun_ms.
  - `struct TaskSummary` — aggregated stats: jobs_run, misses, avg_jitter_ms, max_jitter_ms, overruns.
  - `class DeadlineMonitor` with: `record_job()`, `get_records()`, `get_summaries()`, `print_summary()`, `clear()`.
  - Private: `mutable std::mutex mtx_`, `std::vector<JobRecord> records_`.
- **Key C++ feature:** `mutable` mutex allows locking inside `const` member functions (`get_records`, `get_summaries`).

---

#### 9. `watchdog.h` (33 lines)
- **What it does:** Declares the independent watchdog monitoring thread.
- **Contents:**
  - `using Callback = std::function<void(const std::string&)>` — the recovery callback type.
  - `class Watchdog` with: `start()`, `stop()`, `notify_job_start()`, `notify_job_finish()`, `was_callback_fired()`.
  - Private: `thread_`, `running_` / `active_` / `callback_fired_` (all `std::atomic<bool>`), `mtx_`, `current_task_`, `start_time_`, `timeout_limit_ms_`.
- **Key C++ feature:** Multiple `std::atomic<bool>` fields allow lock-free communication between the dispatcher and watchdog threads.

---

#### 10. `gantt.h` (16 lines)
- **What it does:** Declares output/visualisation functions for Gantt charts and CSV export.
- **Contents:**
  - `export_timeline_csv(filepath, records)` — writes CSV file.
  - `format_text_gantt(records, task_names, total_ms, slot_ms)` — returns string Gantt.
  - `print_text_gantt(records, task_names, total_ms, slot_ms)` — prints to stdout.

---

### Implementation Files (`src/`)

#### 11. `main.cpp` (75 lines)
- **What it does:** CLI entry point and 4 pre-built demo scenarios.
- **Contents:**
  - `run_and_report()` — helper that checks admission, constructs scheduler, runs, prints summary, prints Gantt, exports CSV.
  - `run_scenario_rm()`, `run_scenario_edf()`, `run_scenario_reject()`, `run_scenario_fault()` — 4 named scenarios.
  - `main()` — parses argv[1] to select scenario; also handles `demo <scenario>` alias.
- **Interactions:** Calls all public APIs from policy.h, scheduler.h, gantt.h.

---

#### 12. `policy.cpp` (49 lines)
- **What it does:** Implements admission control mathematics and job priority ordering.
- **Contents:**
  - `JobComparator::operator()` — for RM: compares `period_ms` (shorter = higher priority), tie-break by `release_time` (earlier = higher priority). For EDF: compares `absolute_deadline`, tie-break by `release_time`.
  - `calculate_utilization()` — iterates tasks, accumulates `wcet/period` ratios.
  - `rm_bound(n)` — uses `std::pow(2.0, 1.0/n) - 1.0`, scaled by n.
  - `check_admission()` — picks RM or EDF bound, compares U, prints if requested.

---

#### 13. `scheduler.cpp` (82 lines)
- **What it does:** The core of the system — runs the real-time dispatch loop.
- **Contents:**
  - `spin_busy(ms)` — busy-waits using `steady_clock::now()` loop until time elapses, simulating CPU computation.
  - Constructor — moves task list, sets comparator, initialises empty ready_queue.
  - `run_for(duration_ms)` — starts watchdog, starts dispatcher_thread_, joins thread, stops watchdog.
  - `dispatcher_loop(duration_ms)` — the main loop: release_jobs() → queue check → pop & execute → sleep if idle.
  - `release_jobs(now)` — for each task, pushes all Job instances whose `next_release_[i] <= now`.
  - `execute_job(job)` — arms watchdog, runs body/spin_busy/fault, disarms watchdog, records to monitor.
- **Key design decision:** `running_` is `std::atomic<bool>`, so the destructor can set it `false` safely from the main thread while the dispatcher thread reads it.

---

#### 14. `monitor.cpp` (68 lines)
- **What it does:** Records, aggregates, and prints timing metrics for every executed job.
- **Contents:**
  - `record_job()` — constructs `JobRecord` inline (computing all 11 fields), locks mutex, pushes to `records_`.
  - `get_records()` — locks mutex, returns copy of `records_` (copy protects against concurrent modification).
  - `get_summaries()` — locks mutex, iterates records, accumulates per-task stats via `std::find_if`.
  - `print_summary()` — calls `get_summaries()`, prints formatted table with `std::setw` column alignment.
  - `clear()` — locks mutex, clears `records_`.

---

#### 15. `watchdog.cpp` (49 lines)
- **What it does:** Implements the independent watchdog monitor thread.
- **Contents:**
  - Constructor — stores callback via `std::move`.
  - `start()` — sets `running_ = true`, launches `monitor_loop` on `thread_`.
  - `stop()` — uses `exchange(false)` to atomically check-and-set, then joins thread.
  - `notify_job_start()` — locks mutex, stores task name, computes `timeout_limit_ms_ = wcet_ms * multiplier`, stamps `start_time_`, resets `callback_fired_`, sets `active_ = true`.
  - `notify_job_finish()` — sets `active_ = false` (disarms watchdog).
  - `monitor_loop()` — polls every 2ms, checks elapsed vs. timeout, fires callback exactly once (guarded by `callback_fired_`).

---

#### 16. `gantt.cpp` (51 lines)
- **What it does:** Generates ASCII Gantt charts and CSV output from execution records.
- **Contents:**
  - `export_timeline_csv()` — opens file, writes CSV header + one row per JobRecord.
  - `format_text_gantt()` — iterates 5ms time slots, for each task checks overlap with any JobRecord: `max(slot_start, start_ms) < min(slot_end, finish_ms)` → prints `#` if active, `.` if idle.
  - `print_text_gantt()` — calls `format_text_gantt()`, prints result to stdout.

---

### Tests

#### 17. `tests/tests.cpp` (335 lines)
- **What it does:** Full automated test suite with 24 GoogleTest cases.
- **Test groups:**
  - `TaskTest` (2 tests) — construction with default deadline, explicit deadline & fault flag.
  - `AdmissionTest` (9 tests) — empty utilization, sum of utilization, RM bounds (n=0,1,2,3,10), RM accept/reject, EDF accept/reject.
  - `PriorityQueueTest` (4 tests) — RM shorter period first, RM tie-break, EDF earlier deadline first, EDF tie-break.
  - `MonitorTest` (4 tests) — normal job metrics, overrun and miss detection, summary aggregation, clear().
  - `IntegrationTest` (2 tests) — short RM schedule dispatches tasks, fault injection triggers watchdog callback.
  - `GanttTest` (3 tests — actually 2 here) — text Gantt format, CSV export format and content.
- **Interactions:** Includes all 5 public headers. Uses `GTest::gtest_main`.

---

## 3. "Read the Files in This Order" Learning Plan

Follow this exact sequence to go from zero to fully understanding the project:

| Step | File to Read | What You Learn | Time |
|------|-------------|----------------|------|
| **1** | `docs/01_PROJECT_FLOW.md` | Big picture: pipeline, policies, fault injection | 20 min |
| **2** | `include/rtsched/task.h` | Fundamental data: what a real-time task is | 5 min |
| **3** | `include/rtsched/policy.h` + `src/policy.cpp` | Admission control maths, RM vs EDF priority ordering | 25 min |
| **4** | `include/rtsched/scheduler.h` + `src/scheduler.cpp` | The dispatcher loop: how jobs are released, ordered, executed | 40 min |
| **5** | `include/rtsched/watchdog.h` + `src/watchdog.cpp` | Multi-thread safety, atomic flags, watchdog pattern | 20 min |
| **6** | `include/rtsched/monitor.h` + `src/monitor.cpp` | Metric collection: jitter, response time, overruns | 20 min |
| **7** | `include/rtsched/gantt.h` + `src/gantt.cpp` | Output: Gantt rendering, CSV export | 15 min |
| **8** | `src/main.cpp` | How the 4 scenarios wire everything together | 15 min |
| **9** | `tests/tests.cpp` | How to verify scheduling behaviour with GoogleTest | 30 min |
| **10** | `CMakeLists.txt` + `docs/02_HOW_TO_RUN.md` | Build system: how it all compiles | 20 min |
| **Total** | **Complete Mastery** | Ready to explain every line in an interview | ~3 hours |
