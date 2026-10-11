#include "rtsched/policy.h"
#include <cmath>
#include <iostream>
#include <iomanip>

// Compare two jobs according to the active policy
bool JobComparator::operator()(const Job& a, const Job& b) const {
    if (policy == Policy::RM) {
        if (a.period_ms != b.period_ms) return a.period_ms > b.period_ms;
        return a.release_time > b.release_time; // Break tie with earlier release
    }
    if (a.absolute_deadline != b.absolute_deadline) return a.absolute_deadline > b.absolute_deadline;
    return a.release_time > b.release_time; // Break tie with earlier release
}

// Compute cumulative utilization U = sum(wcet / period) across all tasks
double calculate_utilization(const std::vector<Task>& tasks) {
    double total_u = 0.0;
    for (const auto& task : tasks) {
        if (task.period_ms > 0) {
            total_u += static_cast<double>(task.wcet_ms) / static_cast<double>(task.period_ms);
        }
    }
    return total_u;
}

// Compute Liu & Layland RM bound: U <= n * (2^(1/n) - 1)
double rm_bound(size_t n) {
    if (n == 0) return 0.0;
    const double n_double = static_cast<double>(n);
    return n_double * (std::pow(2.0, 1.0 / n_double) - 1.0);
}

// Check admission control according to selected policy
bool check_admission(const std::vector<Task>& tasks, Policy policy, bool print_output) {
    const double u = calculate_utilization(tasks);
    // Note: The Liu & Layland RM bound is sufficient but not necessary.
    // A task set with U > bound may still be schedulable under RM.
    const double bound = (policy == Policy::RM) ? rm_bound(tasks.size()) : 1.0;
    const bool accepted = (u <= bound);
    if (print_output) {
        std::cout << "[Admission] Policy: " << (policy == Policy::RM ? "RM" : "EDF")
                  << " | U = " << std::fixed << std::setprecision(4) << u
                  << " | Bound = " << bound
                  << " -> " << (accepted ? "ACCEPTED" : "REJECTED") << "\n";
    }
    return accepted;
}
