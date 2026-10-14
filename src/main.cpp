#include "rtsched/task.h"
#include "rtsched/policy.h"
#include "rtsched/scheduler.h"
#include "rtsched/gantt.h"
#include <iostream>
#include <string>
#include <vector>

// Helper to run a schedule, print statistics, draw Gantt chart, and export CSV
void run_and_report(std::vector<Task> tasks, Policy policy, int64_t duration_ms, Watchdog::Callback recovery = nullptr) {
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

int main(int argc, char* argv[]) {
    std::string mode = (argc >= 2) ? argv[1] : "rm";
    if (mode == "demo" && argc >= 3) mode = argv[2];

    if (mode == "rm" || mode == "edf") {
        std::cout << "=== Scenario: " << (mode == "rm" ? "Rate-Monotonic (RM)" : "Earliest Deadline First (EDF)") << " ===\n";
        std::vector<Task> tasks = {Task("T1", 50, 10), Task("T2", 100, 20), Task("T3", 200, 40)};
        run_and_report(tasks, mode == "rm" ? Policy::RM : Policy::EDF, 1000);
    } else if (mode == "reject") {
        std::cout << "=== Scenario: Admission Control (U ~ 0.85) ===\n";
        std::vector<Task> tasks = {Task("T1", 100, 30), Task("T2", 100, 30), Task("T3", 100, 25)};
        check_admission(tasks, Policy::RM, true);
        run_and_report(tasks, Policy::EDF, 1000);
    } else if (mode == "fault") {
        std::cout << "=== Scenario: Fault Injection & Watchdog Recovery ===\n";
        auto recovery = [](const std::string& name) {
            std::cout << "RECOVERY: task " << name << " exceeded limit\n";
        };
        std::vector<Task> tasks = {Task("T1", 50, 10, nullptr, true), Task("T2", 100, 20), Task("T3", 200, 40)};
        run_and_report(tasks, Policy::RM, 1000, recovery);
    } else {
        std::cout << "Usage: rtsched_demo [demo] <rm | edf | reject | fault>\n";
        return 1;
    }
    return 0;
}
