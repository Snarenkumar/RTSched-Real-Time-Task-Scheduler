# RTSched: The Complete Big-Picture Guide to all 25 Test Cases

> **"What are these tests? What goes in? What is expected? How is it calculated? Why does it matter?"**
> This guide connects the dots so you can see the entire project working together like a clockwork engine.

---

## 1. The Big Picture: Why Do We Have These 6 Test Suites?

Think of building a **space rocket autopilot**:
Before launching the rocket, you don't just fire the engines and pray! You test each piece step-by-step:

```
[Level 1: Blueprint]    TaskTest         -> Did we write down the task details correctly?
         ↓
[Level 2: Math Safety]  AdmissionTest    -> Will the CPU crash, or is there enough room for all tasks?
         ↓
[Level 3: Sorting]      PriorityQueueTest-> When 3 tasks arrive at once, who gets the CPU first?
         ↓
[Level 4: Scorekeeper]  MonitorTest      -> Does our stopwatch correctly record delays and missed deadlines?
         ↓
[Level 5: Live Test]    IntegrationTest  -> Turn on real threads and test if the emergency Watchdog works!
         ↓
[Level 6: Visualization]GanttTest        -> Can we draw the timeline chart and export it to Excel?
```

Every single test in `tests.cpp` belongs to one of these 6 levels.

---

## 2. Deep Dive: Suite by Suite Breakdown

---

### SUITE 1: `TaskTest` (The Task Blueprint)
> **Question it answers:** *"When a programmer defines a task, does the system store its settings properly?"*

#### Test 1.1: `DefaultDeadlineToPeriod`
* **What is the Input?**
  A task named `"T1"` that repeats every `50ms` and takes `10ms` of work: `Task t("T1", 50, 10);`
* **What is Expected (`EXPECT_...`)?**
  Notice we didn't specify a deadline. In real-time systems, if you don't give a deadline, **the deadline defaults to the period** (`50ms`).
  ```cpp
  EXPECT_EQ(t.deadline_ms, 50); // Did it automatically set deadline = 50ms?
  EXPECT_FALSE(t.fault);        // Fault injection should be false by default
  ```
* **Why this matters:** If a task repeats every 50ms, it must finish before the next 50ms cycle begins!

#### Test 1.2: `ExplicitDeadlineAndFault`
* **What is the Input?**
  A task where we manually give an urgent deadline of `80ms` (shorter than its 100ms period) and enable the fault flag:
  `Task t("T2", 100, 20, nullptr, true, 80);`
* **What is Expected?**
  ```cpp
  EXPECT_EQ(t.deadline_ms, 80); // Custom deadline 80ms must be respected
  EXPECT_TRUE(t.fault);         // Fault flag must be true
  ```

---

### SUITE 2: `AdmissionTest` (The Math Gatekeeper)
> **Question it answers:** *"Can we mathematically prove the system will never freeze or miss a deadline before turning on the engine?"*

#### Test 2.1 & 2.2: Utilization Formula ($U = \sum \frac{C_i}{T_i}$)
* **What is the Input?**
  Three tasks:
  * T1: takes 10ms every 50ms $\rightarrow 10 / 50 = 0.20$ (20%)
  * T2: takes 20ms every 100ms $\rightarrow 20 / 100 = 0.20$ (20%)
  * T3: takes 40ms every 200ms $\rightarrow 40 / 200 = 0.20$ (20%)
* **How is it Calculated?**
  $$\text{Total } U = 0.20 + 0.20 + 0.20 = \mathbf{0.60} \text{ (60% CPU capacity)}$$
* **What is Expected?**
  `EXPECT_NEAR(calculate_utilization(tasks), 0.60, 1e-6);` -> Verifies the code adds up to exactly 0.60.

#### Tests 2.3 to 2.7: The Liu & Layland RM Bound Formula
$$\text{Bound}(n) = n \cdot \left(2^{1/n} - 1\right)$$
* For $n=1$: Bound = $1.00$ ($100\%$)
* For $n=2$: Bound = $2 \cdot (\sqrt{2}-1) \approx \mathbf{0.8284}$ ($82.8\%$)
* For $n=3$: Bound = $3 \cdot (2^{1/3}-1) \approx \mathbf{0.7798}$ ($\mathbf{78\%}$)
* For $n=10$: Bound = $\approx \mathbf{0.7177}$ ($71.8\%$)
* **What is Expected?**
  `EXPECT_NEAR(rm_bound(3), 0.779763, 1e-5);` -> Proves our C++ `pow()` calculation matches the published 1973 mathematics paper.

#### Tests 2.8 & 2.9: Rate-Monotonic Admission (Accept vs Reject)
* **Test 2.8 (`RMAcceptWhenUnderBound`)**:
  * Input: Tasks with total load $U = 0.60$.
  * Bound for 3 tasks: $0.78$.
  * Check: Is $0.60 \le 0.78$? **YES!**
  * Expected: `EXPECT_TRUE(check_admission(..., Policy::RM));` -> **ACCEPTED**.
* **Test 2.9 (`RMRejectWhenOverBound`)**:
  * Input: Tasks with periods 100ms, runtimes 30ms, 30ms, 25ms.
  * Calculation: $U = 30/100 + 30/100 + 25/100 = \mathbf{0.85}$ ($85\%$ CPU load).
  * Check: Is $0.85 \le 0.78$? **NO! It exceeds the safety limit!**
  * Expected: `EXPECT_FALSE(check_admission(..., Policy::RM));` -> **REJECTED**.

#### Tests 2.10 & 2.11: EDF Admission (100% Capacity)
* **Test 2.10 (`EDFAcceptWhenUnderOne`)**:
  * Input: The exact same 85% load ($U = 0.85$).
  * EDF Rule: As long as $U \le 1.0$ (100%), EDF is guaranteed to schedule all tasks!
  * Check: Is $0.85 \le 1.0$? **YES!**
  * Expected: `EXPECT_TRUE(check_admission(..., Policy::EDF));` -> **ACCEPTED!**
  * *Why this is cool:* Proves why EDF is superior in CPU utilization compared to RM.
* **Test 2.11 (`EDFRejectWhenOverOne`)**:
  * Input: Tasks needing $50ms + 40ms + 20ms = 110ms$ every $100ms$ ($U = 1.10 = 110\%$).
  * Check: No computer can do 110 seconds of work in 100 seconds!
  * Expected: `EXPECT_FALSE(check_admission(..., Policy::EDF));` -> **REJECTED**.

---

### SUITE 3: `PriorityQueueTest` (The Traffic Sorter)
> **Question it answers:** *"When 3 tasks arrive at the exact same moment, who gets to run first, second, and third?"*

#### Test 3.1: RM Rule (Shorter Period = Higher Priority)
* **Input**: 3 jobs arrive at $t=0$:
  * Job 1: Period = 200ms
  * Job 2: Period = 50ms
  * Job 3: Period = 100ms
* **How it sorts**: Rate-Monotonic sorts by shortest period: $50\text{ms} < 100\text{ms} < 200\text{ms}$.
* **What is Expected**:
  ```cpp
  EXPECT_EQ(pq.top().period_ms, 50);  // 1st pop must be 50ms
  pq.pop();
  EXPECT_EQ(pq.top().period_ms, 100); // 2nd pop must be 100ms
  pq.pop();
  EXPECT_EQ(pq.top().period_ms, 200); // 3rd pop must be 200ms
  ```

#### Test 3.2: RM Tie-Breaker
* **What if two tasks have the EXACT same period (e.g. 100ms)?**
* **The Rule**: Whoever arrived at the station first gets served first (First-Come, First-Served).
* **Expected**: `EXPECT_EQ(pq.top().task_index, 0u);` (The earlier arrival wins).

#### Test 3.3: EDF Rule (Nearest Deadline = Higher Priority)
* **Input**: 3 jobs with deadlines at:
  * Job 1: Deadline in 300ms
  * Job 2: Deadline in 80ms (Urgent!)
  * Job 3: Deadline in 150ms
* **How it sorts**: Nearest deadline wins: $80\text{ms} \rightarrow 150\text{ms} \rightarrow 300\text{ms}$.
* **What is Expected**:
  `pq.top()` must pop Job 2 (80ms), then Job 3 (150ms), then Job 1 (300ms).

---

### SUITE 4: `MonitorTest` (The Referee / Scorekeeper)
> **Question it answers:** *"Did our stopwatch correctly log delays (jitter), execution duration, and missed deadlines?"*

#### Test 4.1: Normal On-Time Job
* **Input**:
  * Task: `"T1"`
  * Scheduled Release: `100ms`
  * Actually Started: `102ms`
  * Finished: `112ms`
  * Deadline was: `150ms`
  * Promised Runtime (WCET): `10ms`
* **How it's Calculated**:
  * **Jitter** = $\text{Start} - \text{Release} = 102 - 100 = \mathbf{2ms}$ (waited 2ms in line).
  * **Response Time** = $\text{Finish} - \text{Release} = 112 - 100 = \mathbf{12ms}$.
  * **Overrun** = $(\text{Finish} - \text{Start}) - \text{WCET} = (112 - 102) - 10 = \mathbf{0ms}$ (no overrun).
  * **Deadline Met?** $112 \le 150$ -> **TRUE**.
* **What is Expected**:
  ```cpp
  EXPECT_EQ(recs[0].jitter_ms, 2);
  EXPECT_EQ(recs[0].overrun_ms, 0);
  EXPECT_TRUE(recs[0].deadline_met);
  ```

#### Test 4.2: Fault / Overrun Job
* **Input**:
  * Started at `110ms`, finished at `160ms` (took 50ms, but promised WCET was 20ms!).
  * Deadline was `150ms`.
* **How it's Calculated**:
  * Did it finish before deadline? $160 > 150 \rightarrow$ **NO! Deadline Missed!**
  * Overrun = $50\text{ms} - 20\text{ms} = \mathbf{30ms}$.
* **What is Expected**:
  ```cpp
  EXPECT_EQ(recs[0].overrun_ms, 30);
  EXPECT_FALSE(recs[0].deadline_met); // Caught the violation!
  ```

#### Test 4.3: Summary Aggregation
* **Input**: Log 2 runs for T1 (one with 2ms jitter, one with 8ms jitter) and 1 run for T2 (0ms jitter).
* **Calculation**:
  * T1 Average Jitter = $\frac{2 + 8}{2} = \mathbf{5.0ms}$.
  * T1 Max Jitter = $\mathbf{8ms}$.
  * T1 Misses = $\mathbf{1}$.
* **What is Expected**:
  `EXPECT_NEAR(summaries[0].avg_jitter_ms, 5.0, 1e-6);` -> Verifies the statistics math is flawless.

---

### SUITE 5: `IntegrationTest` (Live End-to-End Execution)
> **Question it answers:** *"When real threads spin on the CPU, does the dispatcher loop execute tasks and does the Watchdog catch freezing tasks?"*

#### Test 5.1: `SchedulerExecution`
* **Input**: Run T1 (50ms period) and T2 (100ms period) for `250ms` real time.
* **How it works**:
  * The dispatcher thread wakes up, pops jobs, runs busy loops, and registers metrics.
  * In 250ms, T1 must run at least 3 to 5 times (250 / 50 = 5).
  * In 250ms, T2 must run at least 1 to 2 times (250 / 100 = 2).
* **What is Expected**:
  ```cpp
  EXPECT_GE(summaries[0].jobs_run, 3);
  EXPECT_GE(summaries[1].jobs_run, 1);
  ```

#### Test 5.2: `WatchdogFaultRecovery`
* **The Scenario**: What if a programmer writes a buggy task that enters an infinite loop or freezes?
* **Input**: We inject a fault (`has_fault = true`) into `FaultyTask`. On its 3rd job, it spins 5 times longer than its WCET limit.
* **How it works**:
  * A separate background **Watchdog thread** counts down the timer.
  * When the task exceeds its limit, the Watchdog barks! It fires our emergency `recovery` callback.
* **What is Expected**:
  ```cpp
  EXPECT_TRUE(callback_called.load());           // Emergency callback triggered!
  EXPECT_EQ(recovered_task, "FaultyTask");       // Caught the exact guilty task!
  EXPECT_GT(summaries[0].overruns, 0);          // Logged as an overrun!
  ```

---

### SUITE 6: `GanttTest` (Visual Timelines & File Export)
> **Question it answers:** *"Can we convert numbers into human-readable charts and export files for analysis?"*

#### Test 6.1: `FormatTextGantt`
* **Input**:
  * T1 ran from 0ms to 10ms.
  * T2 ran from 10ms to 20ms.
  * Slot size = 5ms per character.
* **Calculation**:
  * 0 to 10ms = two 5ms blocks $\rightarrow$ `##`
  * 10 to 20ms = two 5ms blocks $\rightarrow$ `##`
* **What is Expected**:
  ```
  T1 |##..|  (T1 was active for first 10ms, then idle)
  T2 |..##|  (T2 was idle for first 10ms, then active)
  ```
  `EXPECT_TRUE(text.find("T1 |##..|") != std::string::npos);`

#### Test 6.2: `ExportTimelineCSV`
* **Input**: Pass recorded timestamps to `export_timeline_csv("test_timeline.csv", records)`.
* **What is Expected**:
  A real file named `test_timeline.csv` is written to disk. The test opens it and verifies the header and lines:
  ```csv
  task,start_ms,end_ms
  T1,0,10
  T2,10,20
  ```
  Then it deletes the test file so no garbage is left on your drive!

---

## 3. Summary: How All 25 Tests Form a Complete Engine

| Test Suite | Number of Tests | What Component It Verifies | Real-World Guarantee |
| :--- | :---: | :--- | :--- |
| **`TaskTest`** | 2 | `struct Task` | Tasks are configured with valid periods, WCET, and deadlines. |
| **`AdmissionTest`** | 11 | `check_admission()` & Math Formulas | Mathematical guarantee that the CPU will not overload ($U \le \text{Bound}$). |
| **`PriorityQueueTest`** | 4 | `JobComparator` & `std::priority_queue` | Tasks are strictly dispatched in the correct priority order (RM or EDF). |
| **`MonitorTest`** | 4 | `DeadlineMonitor` | Every microsecond of jitter, response time, and miss is recorded accurately. |
| **`IntegrationTest`** | 2 | `Scheduler` & `Watchdog` | Live background threads execute smoothly and recover when tasks crash or freeze. |
| **`GanttTest`** | 2 | `gantt.cpp` & CSV Export | Output visualization and reporting for engineers and logs. |
| **TOTAL** | **25 / 25 Passing** | **Full System** | **A complete, verified, deterministic real-time scheduler!** |
