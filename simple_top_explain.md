# RTSched: Real-Time Task Scheduler (Explained in Plain & Simple Words)

> **"What is this project, what goes in, what comes out, and what happens in between?"**
> If you felt confused by all the theory, this guide is written specifically for you in simple, everyday language with no jargon left unexplained.

---

## 0. Clearing Up the #1 Confusion: "Does this schedule my actual Windows OS?"

**NO! It does NOT touch, modify, or control your Windows operating system.**

* **Your Windows OS** already has its own scheduler inside the Windows Kernel. Windows decides when Google Chrome, Spotify, or VS Code gets the CPU.
* **This project (`RTSched`) is a standalone C++ application / simulation engine.** 
  * It creates its own internal queue of tasks.
  * It runs on a standard C++ thread (`std::thread`).
  * It simulates what an **RTOS (Real-Time Operating System)** like FreeRTOS, VxWorks, or QNX does inside a spacecraft, car, or robot.
* **Why did we build it this way?** 
  Because modifying the Windows kernel requires writing dangerous device drivers that can blue-screen your PC. By writing a real-time scheduler in user-space C++, we can safely simulate, test, measure jitter in microseconds, inject faults, and visualize timelines without risking your computer!

---

## 1. What are Classes, Objects, and Tests? (Beginner Breakdown)

If you saw `tests.cpp` and wondered *"What are these things? Are they classes? Are they objects?"*, here is the exact breakdown:

### A. The Classes & Structs (The Blueprints)
A class or struct in C++ is like an **architectural blueprint for a house**. It defines what data something has and what functions it can perform:
1. **`struct Task`** ([task.h](file:///f:/main%20servers%20under%20productions/RTSched/include/rtsched/task.h)): The blueprint for a task definition. It stores `name`, `period_ms`, `wcet_ms`, `deadline_ms`.
2. **`struct Job`** ([policy.h](file:///f:/main%20servers%20under%20productions/RTSched/include/rtsched/policy.h)): When a task arrives at time $t=50\text{ms}$, it creates a `Job` (instance #1, instance #2, etc.).
3. **`struct JobComparator`** ([policy.h](file:///f:/main%20servers%20under%20productions/RTSched/include/rtsched/policy.h)): The comparison rule. It answers: *"Between Job A and Job B, who gets the CPU first?"*
4. **`class DeadlineMonitor`** ([monitor.h](file:///f:/main%20servers%20under%20productions/RTSched/include/rtsched/monitor.h)): The referee/scorekeeper. Whenever a job finishes, it records: *Did it miss the deadline? How many milliseconds was it delayed?*
5. **`class Watchdog`** ([watchdog.h](file:///f:/main%20servers%20under%20productions/RTSched/include/rtsched/watchdog.h)): The security guard. A separate background thread that rings an alarm if any job takes longer than its promised time.
6. **`class Scheduler`** ([scheduler.h](file:///f:/main%20servers%20under%20productions/RTSched/include/rtsched/scheduler.h)): The overall engine. It holds the task list, runs the priority queue, and dispatches threads.

### B. The Objects (The Actual Things Created from Blueprints)
An **object** is an actual instance made from a blueprint:
```cpp
Task t1("Radar", 50, 10);     // 't1' is an OBJECT of type Task
DeadlineMonitor mon;          // 'mon' is an OBJECT of type DeadlineMonitor
Scheduler sched(tasks, RM);   // 'sched' is an OBJECT of type Scheduler
```

### C. What is a "Test"? (`TEST(...)` in GoogleTest)
When you see:
```cpp
TEST(TaskTest, DefaultDeadlineToPeriod) {
    Task t("T1", 50, 10);
    EXPECT_EQ(t.deadline_ms, 50);
}
```
* **`TEST(...)`** is a GoogleTest macro that creates an automated testing function.
* You don't have to call it manually. When you run `.\build\Release\rtsched_tests.exe`, GoogleTest runs all 25 test functions one after another.
* **`EXPECT_EQ(a, b)`** checks: *"Is `a` equal to `b`?"* If yes, green checkmark. If no, it reports a red failure.

---

## 2. Walkthrough of a Sample Test: Step-by-Step

Let's look at **Test 5.1** from [tests/tests.cpp](file:///f:/main%20servers%20under%20productions/RTSched/tests/tests.cpp#L258-L271) to see exactly how a test works:

```cpp
TEST(IntegrationTest, SchedulerExecution) {
    // STEP 1: CREATE INPUT DATA (Objects)
    std::vector<Task> tasks = {
        Task("T1", 50, 5),   // Task 1: Repeats every 50ms, runs for 5ms
        Task("T2", 100, 5)   // Task 2: Repeats every 100ms, runs for 5ms
    };

    // STEP 2: CREATE THE SCHEDULER OBJECT
    // Tell it to use Policy::RM (Rate-Monotonic: shorter period runs first)
    Scheduler sched(tasks, Policy::RM);

    // STEP 3: RUN THE SCHEDULER FOR 250 MILLISECONDS
    // A background thread wakes up, tracks time, releases jobs, and runs them
    sched.run_for(250);

    // STEP 4: INSPECT THE RESULTS (Output from Monitor)
    const auto summaries = sched.get_monitor().get_summaries();

    // STEP 5: VERIFY WITH ASSERTIONS (Did it do what we expect?)
    // In 250ms, a task with period 50ms should have run at least 3 to 5 times:
    EXPECT_GE(summaries[0].jobs_run, 3);
    
    // In 250ms, a task with period 100ms should have run at least 1 to 2 times:
    EXPECT_GE(summaries[1].jobs_run, 1);
}
```

### What happens inside the computer during those 250ms?
1. At **$t = 0\text{ms}$**: Both `T1` and `T2` enter the Ready Queue.
2. Because `T1`'s period is 50ms and `T2`'s is 100ms, `T1` has higher priority.
3. The scheduler executes `T1` from $0$ to $5\text{ms}$.
4. At **$t = 5\text{ms}$**: `T1` is done. `T2` runs from $5$ to $10\text{ms}$.
5. From $10$ to $50\text{ms}$, CPU is idle.
6. At **$t = 50\text{ms}$**: `T1` arrives again and runs until $55\text{ms}$.
7. At **$t = 100\text{ms}$**: Both `T1` and `T2` arrive again! `T1` runs first, then `T2`.
8. At **$t = 250\text{ms}$**: The run stops. The test checks: did `T1` run at least 3 times? Yes (ran 5 times). Did `T2` run at least once? Yes (ran 2 times). The test **PASSES**!

---

## 3. The 10-Second Real-World Elevator Pitch

Imagine an **airplane autopilot** or a **self-driving car**:
* Every **50 milliseconds**, it must read the camera and obstacle sensors.
* Every **100 milliseconds**, it must calculate speed and steering angle.
* Every **200 milliseconds**, it sends diagnostic data to the dashboard screen.

If the obstacle sensor task runs **10 milliseconds too late**, the car crashes! In real-time computing, being late is just as bad as being wrong.

**This project (`RTSched`) is the CPU "Traffic Police" (Scheduler)**. 
You give it a list of real-world jobs with deadlines, and it:
1. **Calculates upfront with math** whether the CPU can finish everything on time without crashing.
2. **Decides who gets the CPU first** (prioritizing tasks by frequency or urgency).
3. **Runs them in real time** on a background thread.
4. **Guards them with a Watchdog timer** so that if one task freezes, it gets caught immediately.
5. **Prints a visual timeline (Gantt chart)** and statistics showing exactly what happened down to the millisecond.

---

## 4. What is the INPUT? (What you feed into it)

The input is simply a list of tasks you want the computer to run periodically.

### The 4 Main Ingredients of each Task:
| Input Parameter | Plain English Meaning | Example in a Car |
| :--- | :--- | :--- |
| **`name`** | What is this task called? | `"ObstacleCamera"` |
| **`period_ms`** | How often does it repeat? (in milliseconds) | Run every `50 ms` (20 times per second) |
| **`wcet_ms`** | **W**orst-**C**ase **E**xecution **T**ime: How long does the task take to run when working hardest? | Takes `10 ms` of CPU work |
| **`deadline_ms`** | By when must it finish? | Must finish within `50 ms` from when it arrives |

### Plus 2 Configuration Inputs:
* **`Policy`**: Which scheduling rule to follow?
  * **`RM` (Rate-Monotonic)**: "Tasks that repeat most frequently get the highest priority." (Static rule).
  * **`EDF` (Earliest Deadline First)**: "Whoever's deadline is closest right now gets the CPU first." (Dynamic rule).
* **`duration_ms`**: How long to run the simulation? (e.g., `1000 ms` = 1 second).

---

## 5. What is the OUTPUT? (What you see on the screen & file)

When you run the program, it produces **4 clear outputs**:

### Output 1: Admission Check (The Math Verdict)
Before running a single line of code, the system calculates whether the CPU will choke:
```
[Admission] Policy: RM | U = 0.6000 | Bound = 0.7798 -> ACCEPTED
```
* `U = 0.6000`: Your tasks will take 60% of CPU capacity.
* `Bound = 0.7798`: Safety limit for RM policy is ~78%.
* `ACCEPTED`: Math guarantees zero missed deadlines!

### Output 2: Live Execution Summary Table
After running for 1 second, it prints a scorecard of every task:
```
=================== TIMING MONITOR SUMMARY ===================
Task      Jobs      Misses    Avg Jitter(ms) Max Jitter(ms) Overruns  
--------------------------------------------------------------
T1        20        0         10.50          27             0         
T2        10        0         13.80          17             0         
T3        5         0         34.80          37             0         
==============================================================
```
* **Jobs**: How many times each task ran (e.g., T1 ran 20 times in 1000ms because 1000 / 50 = 20).
* **Misses**: `0` (Zero tasks missed their deadline).
* **Jitter**: How long a task had to wait in line before starting.
* **Overruns**: `0` (No task went rogue and ran longer than allowed).

### Output 3: Visual ASCII Timeline (Gantt Chart)
It draws a character-by-character timeline of who was using the CPU:
```
Execution Gantt Chart (1 char = 5 ms):
T1      |##............##....###........###.......##............##...|
T2      |..####................#####................####.............|
T3      |......########.................................########.....|
```
* `#` = Task is running right now on the CPU.
* `.` = Task is idle, waiting, or asleep.

### Output 4: CSV File (`timeline.csv`)
It creates a file you can open in Excel:
```csv
task,start_ms,end_ms
T1,0,10
T2,10,30
T3,30,70
T1,50,60
```

---

## 6. The Math Formulas: Why do we have them & what do they calculate?

Why do we need math before running tasks? **Because you never want an airplane or car to start driving if the CPU is guaranteed to freeze!**

### Formula 1: CPU Utilization ($U$)
> **Question it answers:** "In total, what percentage of the CPU's time do these tasks demand?"

$$\text{Utilization } (U) = \sum \frac{\text{Execution Time } (C_i)}{\text{Period } (T_i)}$$

* $C_i$ = Worst-case execution time (how long it runs).
* $T_i$ = Period (how often it repeats).

#### Step-by-Step Calculation:
* **Task 1**: Takes 10ms every 50ms $\rightarrow \frac{10}{50} = 0.20$ (20% CPU)
* **Task 2**: Takes 20ms every 100ms $\rightarrow \frac{20}{100} = 0.20$ (20% CPU)
* **Task 3**: Takes 40ms every 200ms $\rightarrow \frac{40}{200} = 0.20$ (20% CPU)
* **Total $U$** = $0.20 + 0.20 + 0.20 = \mathbf{0.60}$ (**60% CPU workload**).

---

### Formula 2: Liu & Layland Schedulability Bound (For RM Policy)
> **Question it answers:** "If I prioritize tasks simply by who repeats fastest (RM), is 60% workload safe, or will tasks clash and miss deadlines?"

$$\text{Bound} = n \cdot \left(2^{1/n} - 1\right)$$

Where $n$ is the number of tasks.

* For $n = 1$ task: $1 \cdot (2^{1/1} - 1) = 1.00$ ($100\%$)
* For $n = 2$ tasks: $2 \cdot (2^{1/2} - 1) \approx 0.828$ ($82.8\%$)
* For $n = 3$ tasks: $3 \cdot (2^{1/3} - 1) \approx \mathbf{0.7798}$ ($\mathbf{77.98\%}$)
* For $n \to \infty$ tasks: Approaches $\ln(2) \approx \mathbf{69.3\%}$

#### The Decision:
* Our CPU load $U = 0.60$ (60%).
* The safety bound for 3 tasks is $0.78$ (78%).
* Because **$0.60 \le 0.78$**, the scheduler says: **`ACCEPTED`**. Math guarantees **100% of deadlines will be met**.

---

### Formula 3: EDF Schedulability Bound (For Earliest Deadline First)
> **Question it answers:** "What if I use dynamic priority where the closest deadline always wins?"

$$\text{EDF Bound} = \mathbf{1.00} \text{ (100\%)}$$

* EDF is optimal. If your tasks require up to 100% of the CPU ($U \le 1.0$), EDF can successfully schedule all of them without missing any deadline!

---

### Formula 4: Timing Metrics (How good is the execution?)
1. **Release Jitter**: 
   $$\text{Jitter} = \text{Actual Start Time} - \text{Scheduled Release Time}$$
   *(How long a task sat in line waiting before the CPU picked it up).*
2. **Turnaround Time (Response Time)**:
   $$\text{Response Time} = \text{Finish Time} - \text{Release Time}$$
   *(From the moment the job arrived until it was completely finished).*
3. **Overrun**:
   $$\text{Overrun} = (\text{Finish Time} - \text{Start Time}) - \text{WCET}$$
   *(Did the task take longer than it promised? If $> 0$, it's an overrun).*

---

## 7. The 4 Demo Scenarios Built Into the Project

When you test the program, it has 4 pre-made scenarios to prove each feature:

| Scenario | Command | What it demonstrates | What to look for |
| :--- | :--- | :--- | :--- |
| **1. Standard RM** | `.\build\Release\rtsched_demo.exe rm` | Normal Rate-Monotonic scheduling with 60% CPU load. | All tasks pass with 0 misses. |
| **2. Standard EDF** | `.\build\Release\rtsched_demo.exe edf` | Same tasks running under Earliest-Deadline-First dynamic priority. | Jobs ordered dynamically by nearest deadline. |
| **3. Rejection vs Acceptance** | `.\build\Release\rtsched_demo.exe reject` | Heavy task set with $85\%$ CPU load ($U = 0.85$). | **RM rejects it** ($0.85 > 0.78$), but **EDF accepts it** ($0.85 \le 1.0$)! Shows why EDF is more powerful. |
| **4. Fault & Watchdog** | `.\build\Release\rtsched_demo.exe fault` | A bug is injected into `T1` on its 3rd run so it hangs 5x longer. | Watchdog catches it and triggers `RECOVERY: task T1 exceeded limit`! |

---

## 8. Quick Summary Cheat-Sheet

* **Input**: A list of periodic tasks (Name, Repeat Interval, Computation Workload, Deadline).
* **Formula**: $U = \sum (C_i / T_i)$ must be $\le \text{Bound}$ (78% for RM 3-tasks, 100% for EDF).
* **Processing**: Ready Queue sorts by priority $\rightarrow$ Dispatcher runs top job $\rightarrow$ Watchdog guards timeout $\rightarrow$ Monitor logs timestamps.
* **Output**: Math verdict (`ACCEPTED`), ASCII visual timeline (`Gantt`), Statistics table (`Jitter & Misses`), and Excel-ready file (`timeline.csv`).
