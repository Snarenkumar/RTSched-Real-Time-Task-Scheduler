#pragma once
#include "task.h"
#include <vector>
#include <chrono>

// Supported scheduling policies
enum class Policy { RM, EDF };

// Instantiated job instance in ready queue
struct Job {
    size_t task_index = 0;                                         // Index in task list
    int job_id = 0;                                                // Instance sequence id
    std::chrono::steady_clock::time_point release_time{};          // Ideal release time
    std::chrono::steady_clock::time_point absolute_deadline{};     // Absolute deadline
    int64_t period_ms = 0;                                         // Period for RM comparisons
    int64_t wcet_ms = 0;                                           // WCET parameter
};

// Priority comparator (RM: shorter period; EDF: earlier deadline)
struct JobComparator {
    Policy policy = Policy::RM;
    bool operator()(const Job& a, const Job& b) const;
};

double calculate_utilization(const std::vector<Task>& tasks); // Total U = sum(wcet/period)
double rm_bound(size_t n);                                    // Bound = n*(2^(1/n)-1)
bool check_admission(const std::vector<Task>& tasks, Policy policy, bool print_output = true);
