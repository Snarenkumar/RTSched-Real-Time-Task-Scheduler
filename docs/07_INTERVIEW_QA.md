# Interview Q&A — RTSched

This document contains 50+ interview questions and detailed answers covering every aspect of the RTSched project. Use this to prepare for technical interviews where you will need to explain the design, code, and theory.

---

## Section 1: Project Overview

**Q1: What is RTSched in one sentence?**
RTSched is a user-space, non-preemptive real-time task scheduler written in C++20 that implements Rate-Monotonic and Earliest Deadline First policies with pre-execution admission control, deadline monitoring, watchdog fault detection, and ASCII Gantt/CSV output.

---

**Q2: What problem does RTSched solve?**
In embedded systems (cars, medical devices, aerospace), multiple periodic tasks share a single CPU. Without a principled scheduler, tasks can miss deadlines causing safety failures. RTSched demonstrates how to assign priorities, verify schedulability before running, and detect timing violations at runtime.

---

**Q3: What is in your resume bullet about RTSched and what does each phrase mean?**

- *"priority-driven user-space real-time task scheduler"* — tasks are ordered by priority (not round-robin), it runs as a user application (not kernel code), and tasks have strict timing requirements.
- *"Rate-Monotonic (RMS) and Earliest Deadline First (EDF) policies"* — two distinct scheduling algorithms with different priority assignment rules.
- *"Liu & Layland schedulability bounds (U <= n(2^(1/n) - 1))"* — the mathematical formula from the 1973 paper by Liu & Layland that tells us if a task set is safe to run under RM.
- *"pre-execution admission control"* — we check U <= bound before starting the scheduler, not after a deadline has already been missed.
- *"dedicated dispatcher thread"* — the scheduling loop runs on its own OS thread.
- *"priority queue management"* — we use std::priority_queue with a custom comparator to always dispatch the correct task.
- *"deadline monitor tracking WCET, jitter and overruns"* — three distinct metrics recorded per job: worst-case execution time adherence, release jitter, and execution overruns.
- *"watchdog monitor thread with fault-injection recovery callbacks"* — a second independent thread that fires a callback if a task runs too long.
- *"exported Gantt execution timelines for timing verification"* — ASCII chart and CSV file that prove the schedule ran correctly.

---

## Section 2: Scheduling Theory

**Q4: What is the Liu & Layland bound and where is it in your code?**
The bound U ≤ n * (2^(1/n) - 1) gives the maximum utilization a Rate-Monotonic scheduler can guarantee to schedule. It is implemented in `src/policy.cpp`:
```cpp
double rm_bound(size_t n) {
    const double n_double = static_cast<double>(n);
    return n_double * (std::pow(2.0, 1.0 / n_double) - 1.0);
}
```
For n=3 this gives ≈0.7798. As n→∞ it converges to ln(2) ≈ 0.693.

---

**Q5: Is the RM bound necessary or sufficient?**
It is SUFFICIENT but NOT NECESSARY. If U ≤ bound, you are guaranteed schedulability under RM. But if U > bound, the task set might still be schedulable — the bound just cannot guarantee it. This is why Scenario 3 still runs under EDF even after RM "rejects" it.

---

**Q6: Why is EDF optimal but RM is preferred in automotive?**
EDF is theoretically optimal — if any preemptive scheduler can meet all deadlines, EDF can too. But RM is preferred in automotive because:
1. Fixed priorities are predictable — engineers can reason about failure modes without runtime analysis.
2. RM is simpler to implement and certify for ISO 26262.
3. Tools like AUTOSAR Timing Architect are built around RM analysis.
4. RM's failure mode is well-understood: lowest-priority tasks miss first.

---

**Q7: What is processor utilization and how is it computed in your code?**
U = sum(WCET_i / Period_i) for all tasks. It represents the fraction of CPU time consumed. Implemented in `src/policy.cpp`:
```cpp
double calculate_utilization(const std::vector<Task>& tasks) {
    double total_u = 0.0;
    for (const auto& task : tasks)
        if (task.period_ms > 0)
            total_u += static_cast<double>(task.wcet_ms) / static_cast<double>(task.period_ms);
    return total_u;
}
```

---

**Q8: What is the hyperperiod? What is it for the Scenario 1 task set?**
The hyperperiod is the Least Common Multiple (LCM) of all task periods. After one hyperperiod, the schedule repeats identically.
For T1(50), T2(100), T3(200): LCM(50,100,200) = 200ms. In a 1000ms run, there are exactly 5 hyperperiods.

---

**Q9: What is release jitter and why does it occur in RTSched?**
Jitter = actual start time − ideal release time. In RTSched it occurs because the scheduler is non-preemptive: if T3 (40ms WCET) is executing when T1's release time arrives, T1 must wait until T3 finishes. Even though T1 has higher priority, it cannot interrupt T3. That wait time becomes T1's jitter.

---

**Q10: Why is RTSched non-preemptive? What is the trade-off?**
Non-preemption was chosen for simplicity and correctness: no context switch mechanism is needed, no shared-data race conditions from task interruption. The trade-off is that even the highest-priority task can be delayed by a lower-priority task already executing. Maximum blocking time = WCET of the longest lower-priority task.

---

## Section 3: Code Design

**Q11: Why does the Scheduler use std::thread for the dispatcher?**
The dispatcher must run independently from the calling thread so that `run_for()` can block (join) while the dispatcher loop executes. A dedicated thread also mirrors how real RTOS kernels work — the scheduler has its own execution context.

---

**Q12: Why is running_ declared as std::atomic<bool> instead of plain bool?**
`running_` is read by the dispatcher thread and written by the main thread (in the destructor). Accessing a plain `bool` from two threads simultaneously is undefined behaviour (a data race). `std::atomic<bool>` makes the read/write atomic with no mutex overhead, which is exactly correct for a simple boolean stop-flag.

---

**Q13: Why does DeadlineMonitor have a mutable mutex?**
`get_records()` and `get_summaries()` are logically read-only operations, so they are declared `const`. But they must lock `mtx_` to ensure thread safety (the dispatcher thread writes records concurrently). Locking modifies the mutex object, which is normally not allowed in a `const` function. `mutable` tells the compiler "this member may be modified even in const contexts".

---

**Q14: Explain how the priority queue comparator works.**
`std::priority_queue` is a max-heap — `top()` returns the element for which `comparator(a, b)` returns `false` (i.e., `a` is "greater" than `b`). Our comparator inverts the natural order:
- RM: `return a.period_ms > b.period_ms` — this makes the job with the SMALLER period come out at top.
- EDF: `return a.absolute_deadline > b.absolute_deadline` — job with EARLIER deadline comes out at top.

---

**Q15: What does spin_busy() do and why use it instead of sleep?**
```cpp
void Scheduler::spin_busy(int64_t ms) {
    const auto end = steady_clock::now() + milliseconds(ms);
    while (steady_clock::now() < end) {}  // active CPU spinning
}
```
`sleep_for` is not appropriate for simulating task execution because it yields the CPU — the task would appear to take 0% CPU. Real tasks consume CPU. `spin_busy` keeps the CPU busy (100% on this thread) for exactly `ms` milliseconds, accurately simulating a CPU-bound control task.

---

**Q16: How does fault injection work?**
When `task.fault == true` and `job.job_id == 3` (the third instance of that task):
```cpp
else if (task.fault && job.job_id == 3) spin_busy(task.wcet_ms * 5);
```
The job runs 5× its normal WCET. The Watchdog detects this at 3×WCET and fires the callback. The DeadlineMonitor records the overrun and, if the job finishes after its deadline, a miss.

---

**Q17: How does the Watchdog thread communicate with the Dispatcher thread safely?**
Using a combination of `std::atomic<bool>` (for `active_` and `callback_fired_`) and `std::mutex` (for `current_task_`, `start_time_`, `timeout_limit_ms_`).
- `active_` is atomic: the dispatcher writes it, the watchdog reads it — no lock needed for this boolean.
- Timing parameters need a lock because they are non-atomic types (string, time_point, int64_t) that would be partially read/written without protection.

---

**Q18: Why does the dispatcher sleep in the idle case?**
```cpp
if (earliest > now) std::this_thread::sleep_until(earliest);
```
Without this, the dispatcher would busy-spin (consuming 100% CPU) while waiting for the next task release. `sleep_until` returns the CPU to the OS until the next scheduled event, making the system efficient and leaving CPU available for the Watchdog and other OS processes.

---

## Section 4: C++ Concepts

**Q19: What is the difference between std::atomic and std::mutex? When do you use each?**
- `std::atomic<T>`: Lock-free operations on a single variable. Use for simple flags and counters accessed from multiple threads.
- `std::mutex`: Protects a block of code or multiple variables together. Use when you need to read/modify several related variables atomically as a group.
In RTSched: `running_` uses `atomic<bool>` (single flag, lock-free). The watchdog's `{current_task_, start_time_, timeout_limit_ms_}` group uses `mutex` (multiple related values).

---

**Q20: What is RAII? Where is it used in RTSched?**
Resource Acquisition Is Initialization: resources are acquired in a constructor and released in the destructor, so they are always cleaned up even if an exception is thrown.
RTSched uses RAII via:
- `std::lock_guard<std::mutex>` — acquires lock on construction, releases on destruction.
- `std::thread` — joining in `Scheduler::~Scheduler()` and `Watchdog::~Watchdog()`.
- `std::unique_ptr` (not used here, but the pattern is the same principle).

---

**Q21: What is std::function and why is it used for the task body and watchdog callback?**
`std::function<Signature>` is a type-erased wrapper for any callable matching `Signature`. It stores lambdas, function pointers, or functors interchangeably.
Used in RTSched so that:
- `Task::body` can be `nullptr` (no body, use spin_busy), a lambda, or any callable the user provides.
- `Watchdog::Callback` allows test code to pass a lambda that captures local test variables.

---

**Q22: What does std::chrono::steady_clock guarantee that system_clock does not?**
`steady_clock` is monotonic — it never goes backwards. `system_clock` represents wall-clock time and can jump backwards (due to NTP adjustments, DST changes, or manual clock changes). For timing intervals in a scheduler, monotonic time is essential. If the clock jumped backwards, all `sleep_until` and interval calculations would be wrong.

---

## Section 5: Testing

**Q23: How many tests does RTSched have and what do they cover?**
24 GoogleTest cases across 6 test groups:
- TaskTest (2): default deadline, explicit deadline & fault.
- AdmissionTest (9): empty utilization, sum, RM bounds for n=0/1/2/3/10, RM accept/reject, EDF accept/reject.
- PriorityQueueTest (4): RM shorter period first, RM tie-break, EDF earlier deadline first, EDF tie-break.
- MonitorTest (4): normal metrics, overrun/miss, summary aggregation, clear().
- IntegrationTest (2): short schedule executes tasks, fault injection triggers watchdog.
- GanttTest (2): text Gantt format, CSV export content.

---

**Q24: How do you test the Watchdog callback fires correctly?**
```cpp
std::atomic<bool> callback_called{false};
auto recovery = [&](const std::string& name) { callback_called = true; ... };
Scheduler sched({Task("FaultyTask", 50, 15, nullptr, true)}, Policy::RM, recovery);
sched.run_for(250);
EXPECT_TRUE(callback_called.load());
```
The lambda captures `callback_called` by reference. The Watchdog fires it from its monitor thread. `std::atomic<bool>` ensures the main test thread sees the update safely.

---

**Q25: What would you add to RTSched if you had more time?**
Strong answers:
1. **Preemptive scheduling** — implement context switching via `ucontext_t` or coroutines to make RM/EDF truly optimal.
2. **Sporadic tasks** — extend the model to support tasks with minimum inter-arrival times.
3. **Priority Ceiling Protocol** — eliminate priority inversion in shared resource access.
4. **Response Time Analysis** — implement exact schedulability analysis (not just the sufficient RM bound).
5. **Multi-core dispatching** — partition tasks across cores with global EDF.
6. **Real Gantt GUI** — instead of ASCII, export to a proper timeline visualisation tool.

---

## Section 6: Rapid-Fire Questions

**Q26: What is the RM bound for n=1?** 1.0 (a single task always schedulable if U≤1.0).
**Q27: What is Scenario 3's utilization?** U = 30/100 + 30/100 + 25/100 = 0.85.
**Q28: Why does RM reject Scenario 3 but EDF accepts it?** RM bound(3)≈0.778 < 0.85, EDF bound=1.0 > 0.85.
**Q29: What is the Watchdog timeout multiplier?** 3 (fires at 3×WCET elapsed).
**Q30: How many jobs does T1 run in 1000ms (period=50ms)?** 1000/50 = 20 jobs.
**Q31: What does timeline.csv contain?** task,start_ms,end_ms — one row per job instance.
**Q32: What does `exchange(false)` do in Watchdog::stop()?** Atomically sets `running_` to false AND returns the old value. If old value was true (thread was running), it joins the thread. Prevents double-join.
**Q33: What is ASIL-D?** The highest Automotive Safety Integrity Level — used for airbags, ABS, electric power steering.
**Q34: What is a hyperperiod?** LCM of all task periods. Schedule repeats after one hyperperiod.
**Q35: What is jitter in Scenario 1?** The delay between T1's ideal release and its actual start, caused by non-preemptive blocking on lower-priority tasks already executing.
