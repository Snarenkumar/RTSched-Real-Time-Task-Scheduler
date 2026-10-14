# How to Build, Run & Test RTSched

This document is a complete step-by-step guide to setting up the build environment, compiling the project, running all four demo scenarios, and executing the full test suite on a Windows machine.

---

## 1. Prerequisites

You need the following tools installed before building. All are free.

| Tool | Minimum Version | Purpose |
|------|----------------|---------|
| **Visual Studio Build Tools** | 2019 (v16) or newer | C++ compiler (MSVC cl.exe) |
| **CMake** | 3.20+ | Build system configuration |
| **Ninja** (optional) | Any | Faster builds (included in VS Build Tools) |
| **Git** | Any | Clone repo, optional |
| **Internet connection** | — | CMake downloads GoogleTest automatically via FetchContent |

### Checking Your Tools

Open a **Developer PowerShell for VS** (or a normal PowerShell after running `vcvars64.bat`):

```powershell
cmake --version   # Should print: cmake version 3.20.x or higher
cl                # Should print: Microsoft C/C++ Optimizing Compiler
```

---

## 2. Getting the Code

If you already have the folder, skip this step.

```powershell
git clone https://github.com/Snarenkumar/RTSched-Real-Time-Task-Scheduler.git
cd RTSched-Real-Time-Task-Scheduler
```

---

## 3. Building the Project

### Step 1 — Configure the build tree

Open a **Developer PowerShell for VS 2019/2022** (important: you need MSVC on PATH).

```powershell
cd "f:\main servers under productions\RTSched"
cmake -B build
```

What happens:
- CMake reads `CMakeLists.txt`.
- Detects your compiler (MSVC).
- Downloads GoogleTest v1.14.0 from GitHub automatically.
- Generates build files in the `build/` folder.

Expected output excerpt:
```
-- The CXX compiler identification is MSVC 19.x
-- Configuring done
-- Build files have been written to: .../RTSched/build
```

### Step 2 — Compile everything

```powershell
cmake --build build --config Release
```

This compiles:
- `rtsched` — the core static library (`rtsched.lib`)
- `rtsched_demo` — the CLI demonstration executable
- `rtsched_tests` — the GoogleTest executable

Expected output:
```
Build succeeded.
    0 Warning(s)
    0 Error(s)
```

---

## 4. Running the Demo Scenarios

The `rtsched_demo` executable accepts one argument selecting the scenario.

### Scenario 1: Rate-Monotonic (RM) — Schedulable

```powershell
.\build\Release\rtsched_demo.exe rm
```

Task set: T1(50ms/10ms), T2(100ms/20ms), T3(200ms/40ms). U=0.60, bound≈0.78. **Accepted.**

Sample output:
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
T1      |##............##....###.........###.......##............|
T2      |..####................#####.................####.........|
T3      |......########..................................########..|

[Output] Exported execution timeline to timeline.csv
```

---

### Scenario 2: Earliest Deadline First (EDF)

```powershell
.\build\Release\rtsched_demo.exe edf
```

Same task set under EDF scheduling. EDF is optimal so deadline adherence may be better under high load.

---

### Scenario 3: Admission Comparison (U ≈ 0.85)

```powershell
.\build\Release\rtsched_demo.exe reject
```

Task set: T1(100/30), T2(100/30), T3(100/25). U=0.85.
- RM bound(3)≈0.78 → **REJECTED** by RM.
- EDF bound=1.0 → **ACCEPTED** by EDF. Schedule runs under EDF.

Sample output:
```
=== Scenario 3: Admission Control Comparison (U ~ 0.85) ===
[Admission] Policy: RM | U = 0.8500 | Bound = 0.7798 -> REJECTED
[Admission] Policy: EDF | U = 0.8500 | Bound = 1.0000 -> ACCEPTED
```

---

### Scenario 4: Fault Injection & Watchdog Recovery

```powershell
.\build\Release\rtsched_demo.exe fault
```

T1 has `fault=true`. On job instance #3, T1 deliberately runs 5× its WCET.
The Watchdog fires after 3×WCET (30ms) and calls the recovery callback.

Sample output:
```
=== Scenario 4: Fault Injection & Watchdog Recovery ===
[Admission] Policy: RM | U = 0.6000 | Bound = 0.7798 -> ACCEPTED
RECOVERY: task T1 exceeded limit

Task      Jobs  Misses  Avg Jitter(ms)  Max Jitter(ms)  Overruns
T1        20    1       12.85           32              1
```

---

## 5. Running the Test Suite

### Run all 24 tests directly

```powershell
.\build\Release\rtsched_tests.exe
```

### Run through CTest (summary mode)

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Expected output:
```
Test project F:/main servers under productions/RTSched/build
    Start 1: RTSchedTests
1/1 Test #1: RTSchedTests .....................   Passed   X.XX sec

100% tests passed, 0 tests failed out of 1
```

### Run with verbose output (see each test name)

```powershell
.\build\Release\rtsched_tests.exe --gtest_color=yes
```

---

## 6. Testing a Custom Task Set

You can write a quick ad-hoc test in the `tests/` folder or create a temporary `.cpp` file in the project root:

```cpp
// my_custom_test.cpp — place in tests/ or project root
#include "rtsched/task.h"
#include "rtsched/policy.h"
#include "rtsched/scheduler.h"
#include "rtsched/gantt.h"
#include <iostream>
#include <vector>

int main() {
    std::vector<Task> tasks = {
        Task("A", 30, 8),   // 8ms every 30ms
        Task("B", 60, 15),  // 15ms every 60ms
    };

    if (!check_admission(tasks, Policy::RM)) {
        std::cout << "Task set rejected!\n";
        return 1;
    }

    Scheduler sched(tasks, Policy::RM);
    sched.run_for(500);  // run for 500ms
    sched.get_monitor().print_summary();

    std::vector<std::string> names = {"A", "B"};
    print_text_gantt(sched.get_monitor().get_records(), names, 500);
    export_timeline_csv("my_custom_timeline.csv", sched.get_monitor().get_records());
    return 0;
}
```

Add this target to `CMakeLists.txt`:
```cmake
add_executable(my_custom_test my_custom_test.cpp)
target_link_libraries(my_custom_test PRIVATE rtsched)
```
Then rebuild and run `.\build\Release\my_custom_test.exe`.

---

## 7. Checking the CSV Export

After any scenario run, `timeline.csv` appears in your working directory:

```
task,start_ms,end_ms
T1,0,10
T2,10,30
T3,30,70
T1,50,60
...
```

You can open this in Excel or process with Python:
```python
import pandas as pd
import matplotlib.pyplot as plt
df = pd.read_csv("timeline.csv")
for _, row in df.iterrows():
    plt.barh(row["task"], row["end_ms"] - row["start_ms"], left=row["start_ms"])
plt.xlabel("Time (ms)")
plt.title("RTSched Execution Gantt")
plt.show()
```

---

## 8. Common Build Errors & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `cmake` not found | CMake not on PATH | Open a **Developer PowerShell for VS** or add CMake to PATH |
| `cl.exe` not found | MSVC not on PATH | Use **Developer PowerShell for VS** or run `vcvars64.bat` |
| FetchContent fails | No internet access | Pre-download GoogleTest zip and set `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` |
| `/W4 /WX` compile errors | New warning from compiler | Read the warning carefully; fix it in the source (all warnings are real issues) |
| Tests fail with timing errors | System under heavy load | Re-run; timing-sensitive tests use physical `sleep_until` and depend on system load |

---

## 9. Build Output Files

After a successful build, these files are created:

```
build/Release/
├── rtsched.lib          <- Core static library
├── rtsched_demo.exe     <- CLI demo executable
└── rtsched_tests.exe    <- GoogleTest executable
```
