# Line-by-Line Execution Walkthrough

This document traces exactly what happens inside RTSched when you run Scenario 1 (Rate-Monotonic) with task set T1(50/10), T2(100/20), T3(200/40) for 1000ms.
Every function call, data transformation, and thread interaction is shown in plain English.

---

## 1. The Input

You type:
```
.\build\Release\rtsched_demo.exe rm
```

`main()` in `src/main.cpp` reads `argv[1] == "rm"` and calls `run_scenario_rm()`.

---

## 2. Task Construction

```cpp
std::vector<Task> tasks = {
    Task("T1", 50, 10),    // period=50ms, wcet=10ms, deadline defaults to 50ms
    Task("T2", 100, 20),   // period=100ms, wcet=20ms, deadline defaults to 100ms
    Task("T3", 200, 40)    // period=200ms, wcet=40ms, deadline defaults to 200ms
};
```

Each `Task` constructor checks if deadline <=0, and since none was given, sets `deadline_ms = period_ms`.

After construction, memory looks like:
```
tasks[0]: name="T1", period=50, wcet=10, deadline=50, fault=false
tasks[1]: name="T2", period=100, wcet=20, deadline=100, fault=false
tasks[2]: name="T3", period=200, wcet=40, deadline=200, fault=false
```

---

## 3. Admission Control

`run_and_report()` calls `check_admission(tasks, Policy::RM, true)`:

### Inside `calculate_utilization(tasks)`:
```
U_T1 = 10.0 / 50.0  = 0.200
U_T2 = 20.0 / 100.0 = 0.200
U_T3 = 40.0 / 200.0 = 0.200
Total U = 0.600
```

### Inside `rm_bound(3)`:
```
n = 3.0
pow(2.0, 1.0/3.0) = pow(2.0, 0.333) = 1.2599...
1.2599 - 1.0 = 0.2599...
3.0 * 0.2599 = 0.7798...
Bound = 0.7798
```

### Decision:
```
U=0.6000 <= Bound=0.7798 -> ACCEPTED
```

Printed: `[Admission] Policy: RM | U = 0.6000 | Bound = 0.7798 -> ACCEPTED`

---

## 4. Scheduler Construction

```cpp
Scheduler sched(tasks, Policy::RM, nullptr);
```

Inside the constructor:
- `tasks_` = copy of task vector (3 tasks stored).
- `comparator_` = `JobComparator{Policy::RM}`.
- `ready_queue_` = empty priority_queue using `comparator_`.
- `watchdog_` = `Watchdog(nullptr)` — no recovery callback.
- `next_release_` = `vector<time_point>(3)` — 3 entries, uninitialised.
- `job_ids_` = `{1, 1, 1}` — first job of each task starts at id=1.

---

## 5. Running the Schedule

`sched.run_for(1000)` is called.

### Step 5.1 — Watchdog Starts
```cpp
watchdog_.start();
// running_ = true
// thread_ = std::thread(&Watchdog::monitor_loop, this)
```
A new OS thread is created, running `Watchdog::monitor_loop()`. It immediately enters its while-loop, sleeping 2ms between checks. Since `active_=false`, it just loops doing nothing.

### Step 5.2 — Dispatcher Thread Starts
```cpp
running_ = true;
dispatcher_thread_ = std::thread(&Scheduler::dispatcher_loop, this, 1000);
```
A second OS thread is created, running `dispatcher_loop(1000)`.

---

## 6. Inside dispatcher_loop(1000)

```
base_time_ = steady_clock::now()    <- t = 0ms (our time origin)
stop_time  = base_time_ + 1000ms   <- t = 1000ms
next_release_[0] = base_time_       <- T1 next release at t=0ms
next_release_[1] = base_time_       <- T2 next release at t=0ms
next_release_[2] = base_time_       <- T3 next release at t=0ms
```

### Iteration 1: t ≈ 0ms

#### release_jobs(now ≈ 0ms):
```
T1: next_release_[0]=0ms <= now=0ms -> push Job{index=0, id=1, release=0ms, deadline=50ms, period=50}
    next_release_[0] = 0 + 50 = 50ms
T2: next_release_[1]=0ms <= now=0ms -> push Job{index=1, id=1, release=0ms, deadline=100ms, period=100}
    next_release_[1] = 0 + 100 = 100ms
T3: next_release_[2]=0ms <= now=0ms -> push Job{index=2, id=1, release=0ms, deadline=200ms, period=200}
    next_release_[2] = 0 + 200 = 200ms
```

Ready queue now holds 3 jobs. JobComparator (RM) orders by period ascending:
```
Top: T1 (period=50) <- shortest period, highest priority
     T2 (period=100)
     T3 (period=200)
```

#### execute_job(T1, job_id=1):
```
start_tp = steady_clock::now()  <- t ≈ 0ms
watchdog_.notify_job_start("T1", wcet=10, multiplier=3)
    -> timeout_limit_ms_ = 30ms
    -> start_time_ = now
    -> active_ = true
spin_busy(10ms)  <- CPU busy-waits for 10ms
watchdog_.notify_job_finish()
    -> active_ = false
finish_tp = steady_clock::now() <- t ≈ 10ms
monitor_.record_job("T1", 1, release=0, start=0, finish=10, deadline=50, wcet=10)
    -> jitter=0, response=10, overrun=0, deadline_met=true
```

### Iteration 2: t ≈ 10ms

release_jobs(now≈10ms): No task is due yet (T1 next at 50ms, T2 at 100ms, T3 at 200ms).
Queue still has T2 (period=100) and T3 (period=200).

#### execute_job(T2, job_id=1):
```
spin_busy(20ms)  <- T2 runs 20ms
finish_tp ≈ 30ms
monitor_.record_job("T2", 1, release=0, start=10, finish=30, deadline=100, wcet=20)
    -> jitter=10, response=30, overrun=0, deadline_met=true
```

### Iteration 3: t ≈ 30ms

T3 is the only job left in queue.
#### execute_job(T3, job_id=1):
```
spin_busy(40ms)  <- T3 runs 40ms
finish_tp ≈ 70ms
monitor_.record_job("T3", 1, release=0, start=30, finish=70, deadline=200, wcet=40)
    -> jitter=30, response=70, overrun=0, deadline_met=true
```

### Iteration 4: t ≈ 70ms

Queue is now empty. Compute earliest next release:
```
min(next_release_) = min(50ms, 100ms, 200ms) = 50ms
50ms < now=70ms  -> T1 is already overdue!
```

Actually, `next_release_[0]=50ms <= now=70ms`, so in the next `release_jobs()` call T1 will be immediately released.

### Iteration 5: t ≈ 70ms (next loop pass)

#### release_jobs(now≈70ms):
```
T1: next_release_=50ms <= 70ms -> push Job{T1, id=2, release=50ms, deadline=100ms}
    next_release_[0] = 50 + 50 = 100ms
T2: next_release_=100ms > 70ms -> skip
T3: next_release_=200ms > 70ms -> skip
```

execute_job(T1, job_id=2): runs at t≈70ms, finishes at t≈80ms, jitter=20ms (started 20ms after ideal release of 50ms).

...and so on for the full 1000ms.

---

## 7. Watchdog Thread Behaviour (Scenario 1)

Throughout Scenario 1, no task has `fault=true`. Every job runs at most `wcet_ms` milliseconds. The Watchdog's `timeout_limit_ms_ = wcet_ms * 3`, so:

- T1: timeout = 30ms. T1 always finishes in ~10ms. Watchdog never fires.
- T2: timeout = 60ms. T2 always finishes in ~20ms. Watchdog never fires.
- T3: timeout = 120ms. T3 always finishes in ~40ms. Watchdog never fires.

The recovery callback is never called in Scenario 1.

---

## 8. Monitor Summary After 1000ms

After `dispatcher_loop` finishes, `get_monitor().print_summary()` aggregates all records.

Approximated from a real run:

```
Task  Jobs  Misses  Avg Jitter(ms)  Max Jitter(ms)  Overruns
T1    20    0       11.45           30              0
T2    10    0       16.30           25              0
T3    5     0       36.80           40              0
```

**Why is there jitter at all?**
Non-preemptive scheduling: T1 is the highest priority task, but if T3 (40ms WCET) is executing when T1's next period arrives, T1 must wait 40ms. That wait time becomes T1's jitter. This is a fundamental property of non-preemptive real-time scheduling — even the highest-priority task can be delayed by a lower-priority one that is already running.

---

## 9. Gantt Chart Rendering

`print_text_gantt(records, names, 1000, 5)`:

For each task name (T1, T2, T3), for each 5ms slot from t=0 to t=1000:
```
Check overlap: max(slot_start, job.start_ms) < min(slot_end, job.finish_ms)
If true -> print '#'
Else    -> print '.'
```

Example for t=0 to t=70 (14 slots, each 5ms):
```
T1  | # # . . . . . . . . . . . . |   (active 0-10ms = slots 0,1)
T2  | . . # # # # . . . . . . . . |   (active 10-30ms = slots 2,3,4,5)
T3  | . . . . . . # # # # # # # # |   (active 30-70ms = slots 6-13)
```

---

## 10. CSV Export

`export_timeline_csv("timeline.csv", records)`:

Opens file, writes header, then one line per JobRecord:
```
task,start_ms,end_ms
T1,0,10
T2,10,30
T3,30,70
T1,70,80
T2,100,120
...
```
