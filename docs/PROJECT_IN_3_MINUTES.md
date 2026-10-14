# RTSched: The Whole Project in 3 Minutes
*(Everything you need to know, without the overwhelming complexity)*

---

## 1. Don't Panic! 80% of the Code is Just "Packaging"

If the project looks scary with 15 files, here is the secret: **Only 3 files do the real work.** Everything else is just packaging, safety checks, or pretty printing.

```
WHAT ACTUALLY MATTERS (The Core 20%):
1. task.h      -> Holds 3 numbers: period, execution time, deadline.
2. policy.cpp  -> Decides who goes first (shortest period wins).
3. scheduler.cpp -> Takes the top job and runs it.

WHAT YOU CAN SAFELY IGNORE (The Extra 80%):
- gantt.cpp    -> Just draws '#' characters on screen. (Cosmetic)
- watchdog.cpp -> Just a backup timer in case a task freezes. (Safety feature)
- monitor.cpp  -> Just a stopwatch calculating averages. (Logging)
- tests.cpp    -> Just verifies the math with EXPECT_EQ. (Automated checks)
```

---

## 2. The Entire Project in 3 Simple Steps

Strip away the classes, templates, and threads. Here is what the computer actually does:

### Step 1: The Math Check (Are we overloading the CPU?)
Before starting, we add up the work:
$$\text{Workload} = \frac{\text{Task 1 Work}}{\text{Task 1 Period}} + \frac{\text{Task 2 Work}}{\text{Task 2 Period}} + \frac{\text{Task 3 Work}}{\text{Task 3 Period}}$$
* If your tasks take **60%** of CPU time $\rightarrow$ **Safe! Start the engine.**
* If your tasks demand **120%** of CPU time $\rightarrow$ **Impossible! Warn the user.**

### Step 2: The Priority Rule (Who gets the CPU?)
When 3 tasks want the CPU at the exact same second:
* **Rate-Monotonic Rule (RM)**: The task that repeats the **fastest** gets the CPU first.
  *(Example: A 50ms task beats a 100ms task, which beats a 200ms task).*
* **Earliest Deadline First (EDF)**: The task whose **deadline is closest right now** gets the CPU first.

### Step 3: Run the Task
The scheduler picks the winner, lets it run for its promised time, and then picks the next one.

**That's it. That is the entire project!**

---

## 3. The 30-Second Summary You Can Say Out Loud

If anyone asks you *"What is this project?"*, memorize these 3 sentences:

> *"RTSched is a real-time task scheduler in C++. It takes periodic tasks—like sensor reads and motor controls in a robot—and ensures they finish before their deadlines. It mathematically checks if the CPU can handle the workload, prioritizes urgent tasks using Rate-Monotonic and EDF algorithms, and uses a watchdog timer to catch any task that freezes."*

---

## 4. The 3 Numbers You Should Remember

If someone asks for an example, use these exact numbers from the demo:

| Task | What it represents | Repeats Every (`T`) | Takes CPU Time (`C`) | CPU Load (`C / T`) |
| :--- | :--- | :--- | :--- | :--- |
| **`T1`** | Fast Sensor (Radar) | **50 ms** | **10 ms** | $10 / 50 = \mathbf{20\%}$ |
| **`T2`** | Motor Controller | **100 ms** | **20 ms** | $20 / 100 = \mathbf{20\%}$ |
| **`T3`** | Display Logger | **200 ms** | **40 ms** | $40 / 200 = \mathbf{20\%}$ |

* **Total CPU Load**: $20\% + 20\% + 20\% = \mathbf{60\%}$.
* **Safety Limit (Bound)**: For 3 tasks, the limit is **$78\%$**.
* **Result**: $60\% \le 78\% \rightarrow$ **ACCEPTED (Zero missed deadlines!)**

---

## 5. Cheat Sheet of Code Names to Everyday Words

| Fancy Code Name | What It Really Is in Simple English |
| :--- | :--- |
| `Task` | A job profile (Name, how often it repeats, how long it runs). |
| `Job` | One specific instance of a task running today. |
| `JobComparator` | The rulebook that sorts jobs: *"Shortest period goes first"*. |
| `check_admission()` | A calculator checking: is Total CPU workload $\le 78\%$? |
| `spin_busy(ms)` | Burning CPU cycles for $X$ milliseconds to simulate real work. |
| `Watchdog` | An alarm clock that rings if a task takes too long. |
| `DeadlineMonitor` | A notepad that writes down: *"Task 1 finished on time"*. |
