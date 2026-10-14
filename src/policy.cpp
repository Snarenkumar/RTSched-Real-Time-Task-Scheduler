#include "rtsched/policy.h"
#include <cmath>
#include <iostream>
#include <iomanip>

// Priority rule:
// - RM (Rate-Monotonic): Shorter period = higher priority.
// - EDF (Earliest Deadline First): Earlier deadline = higher priority.
// (Note: in C++ priority_queue, returning 'true' means 'a' has lower priority than 'b')
bool JobComparator::operator()(const Job& a, const Job& b) const {
    if (policy == Policy::RM) {
        if (a.period_ms != b.period_ms) return a.period_ms > b.period_ms;
        return a.release_time > b.release_time; // Tie-breaker: earlier arrival wins
    }
    if (a.absolute_deadline != b.absolute_deadline) return a.absolute_deadline > b.absolute_deadline;
    return a.release_time > b.release_time;     // Tie-breaker: earlier arrival wins
}

// Total CPU load: U = sum(wcet / period)
double calculate_utilization(const std::vector<Task>& tasks) {
    double total = 0.0;
    for (const auto& t : tasks) {
        if (t.period_ms > 0) total += static_cast<double>(t.wcet_ms) / t.period_ms;
    }
    return total;
}

// Liu & Layland RM Bound: n * (2^(1/n) - 1)
double rm_bound(size_t n) {
    if (n == 0) return 0.0;
    return static_cast<double>(n) * (std::pow(2.0, 1.0 / static_cast<double>(n)) - 1.0);
}

// Admission Check: Is CPU load <= safety limit?
bool check_admission(const std::vector<Task>& tasks, Policy policy, bool print_output) {
    const double u = calculate_utilization(tasks);
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
