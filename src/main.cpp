#include "rtsched/task.h"
#include "rtsched/policy.h"
#include "rtsched/scheduler.h"
#include "rtsched/gantt.h"
#include <iostream>
#include <string>
#include <vector>

// Helper to run schedule, print metrics, render Gantt, and export CSV
void run_and_report(std::vector<Task> tasks, Policy policy, int64_t duration_ms, Watchdog::Callback recovery) {
    check_admission(tasks, policy, true);
    Scheduler sched(tasks, policy, std::move(recovery));
    sched.run_for(duration_ms);
    sched.get_monitor().print_summary();
    std::vector<std::string> names;
    for (const auto& t : tasks) names.push_back(t.name);
    print_text_gantt(sched.get_monitor().get_records(), names, duration_ms);
    export_timeline_csv("timeline.csv", sched.get_monitor().get_records());
    std::cout << "[Output] Exported execution timeline to timeline.csv\n\n";
}

// Scenario 1: Rate-Monotonic scheduling with schedulable task set
void run_scenario_rm() {
    std::cout << "=== Scenario 1: Rate-Monotonic (RM) Schedulable Set ===\n";
    std::vector<Task> tasks = {Task("T1", 50, 10), Task("T2", 100, 20), Task("T3", 200, 40)};
    run_and_report(tasks, Policy::RM, 1000, nullptr);
}

// Scenario 2: Earliest Deadline First scheduling with schedulable task set
void run_scenario_edf() {
    std::cout << "=== Scenario 2: Earliest Deadline First (EDF) Schedulable Set ===\n";
    std::vector<Task> tasks = {Task("T1", 50, 10), Task("T2", 100, 20), Task("T3", 200, 40)};
    run_and_report(tasks, Policy::EDF, 1000, nullptr);
}

// Scenario 3: Task set with U ~ 0.85 where RM rejects but EDF accepts
void run_scenario_reject() {
    std::cout << "=== Scenario 3: Admission Control Comparison (U ~ 0.85) ===\n";
    std::vector<Task> tasks = {Task("T1", 100, 30), Task("T2", 100, 30), Task("T3", 100, 25)};
    // Liu & Layland RM bound is sufficient but not necessary
    // RM bound for n=3 is ~0.7798 (rejects U=0.85), but EDF bound is 1.0 (accepts)
    check_admission(tasks, Policy::RM, true);
    run_and_report(tasks, Policy::EDF, 1000, nullptr);
}

// Scenario 4: Fault injection causing execution overrun and watchdog recovery
void run_scenario_fault() {
    std::cout << "=== Scenario 4: Fault Injection & Watchdog Recovery ===\n";
    auto recovery = [](const std::string& name) {
        std::cout << "RECOVERY: task " << name << " exceeded limit\n";
    };
    std::vector<Task> tasks = {
        Task("T1", 50, 10, nullptr, true), Task("T2", 100, 20), Task("T3", 200, 40)
    };
    run_and_report(tasks, Policy::RM, 1000, recovery);
}

// Entry point parsing CLI arguments to run requested demo scenario
int main(int argc, char* argv[]) {
    std::string mode = "rm";
    if (argc >= 2) {
        mode = argv[1];
        if (mode == "demo" && argc >= 3) mode = argv[2];
    }
    if (mode == "rm") run_scenario_rm();
    else if (mode == "edf") run_scenario_edf();
    else if (mode == "reject") run_scenario_reject();
    else if (mode == "fault") run_scenario_fault();
    else {
        std::cout << "Usage: rtsched_demo [demo] <rm | edf | reject | fault>\n";
        return 1;
    }
    return 0;
}
