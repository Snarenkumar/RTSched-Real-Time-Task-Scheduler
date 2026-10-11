#pragma once
#include "task.h"
#include "policy.h"
#include "monitor.h"
#include "watchdog.h"
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>

// RTSched is strictly non-preemptive: once a job starts, it runs to completion without interruption.
class Scheduler {
public:
    Scheduler(std::vector<Task> tasks, Policy policy, Watchdog::Callback recovery_cb = nullptr);
    ~Scheduler();
    void run_for(int64_t duration_ms);                       // Executes schedule for duration_ms
    const DeadlineMonitor& get_monitor() const;              // Read-only monitor access
    DeadlineMonitor& get_monitor();                          // Mutable monitor access
    const std::vector<Task>& get_tasks() const;              // Task definitions
    static void spin_busy(int64_t ms);                       // Simulates CPU execution workload
private:
    void dispatcher_loop(int64_t duration_ms);               // Core dispatcher loop
    void release_jobs(const std::chrono::steady_clock::time_point& now);
    void execute_job(const Job& job);
    std::vector<Task> tasks_;
    Policy policy_;
    JobComparator comparator_;
    std::priority_queue<Job, std::vector<Job>, JobComparator> ready_queue_;
    DeadlineMonitor monitor_;
    Watchdog watchdog_;
    std::thread dispatcher_thread_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point base_time_{};
    std::vector<std::chrono::steady_clock::time_point> next_release_;
    std::vector<int> job_ids_;
};
