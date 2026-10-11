#include "rtsched/scheduler.h"
#include <algorithm>

// Busy-wait loop consuming CPU cycles to simulate deterministic computation workload
void Scheduler::spin_busy(int64_t ms) {
    const auto end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end_time) {} // Active CPU spinning
}

// Construct scheduler with task specifications, scheduling policy, and fault callback
Scheduler::Scheduler(std::vector<Task> tasks, Policy policy, Watchdog::Callback recovery_cb)
    : tasks_(std::move(tasks)), policy_(policy), comparator_{policy}, ready_queue_(comparator_),
      watchdog_(std::move(recovery_cb)), next_release_(tasks_.size()), job_ids_(tasks_.size(), 1) {}

Scheduler::~Scheduler() {
    running_ = false;
    if (dispatcher_thread_.joinable()) dispatcher_thread_.join();
}

// Start watchdog and dispatcher thread, executing the schedule for duration_ms
void Scheduler::run_for(int64_t duration_ms) {
    watchdog_.start();
    running_ = true;
    dispatcher_thread_ = std::thread(&Scheduler::dispatcher_loop, this, duration_ms);
    if (dispatcher_thread_.joinable()) dispatcher_thread_.join();
    watchdog_.stop();
}

const DeadlineMonitor& Scheduler::get_monitor() const { return monitor_; }
DeadlineMonitor& Scheduler::get_monitor() { return monitor_; }
const std::vector<Task>& Scheduler::get_tasks() const { return tasks_; }

// Release periodic jobs whose arrival timestamps have passed
void Scheduler::release_jobs(const std::chrono::steady_clock::time_point& now) {
    for (size_t i = 0; i < tasks_.size(); ++i) {
        while (next_release_[i] <= now) {
            Job job{i, job_ids_[i]++, next_release_[i],
                    next_release_[i] + std::chrono::milliseconds(tasks_[i].deadline_ms),
                    tasks_[i].period_ms, tasks_[i].wcet_ms};
            ready_queue_.push(job);
            next_release_[i] += std::chrono::milliseconds(tasks_[i].period_ms);
        }
    }
}

// Non-preemptively execute job, notifying watchdog and recording metrics
void Scheduler::execute_job(const Job& job) {
    const auto& task = tasks_[job.task_index];
    const auto start_tp = std::chrono::steady_clock::now();
    watchdog_.notify_job_start(task.name, task.wcet_ms);
    if (task.body) task.body();
    else if (task.fault && job.job_id == 3) spin_busy(task.wcet_ms * 5); // Injected fault on job 3
    else spin_busy(task.wcet_ms);
    watchdog_.notify_job_finish();
    const auto finish_tp = std::chrono::steady_clock::now();
    const int64_t rel_ms = std::chrono::duration_cast<std::chrono::milliseconds>(job.release_time - base_time_).count();
    const int64_t start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(start_tp - base_time_).count();
    const int64_t finish_ms = std::chrono::duration_cast<std::chrono::milliseconds>(finish_tp - base_time_).count();
    const int64_t dead_ms = std::chrono::duration_cast<std::chrono::milliseconds>(job.absolute_deadline - base_time_).count();
    monitor_.record_job(task.name, job.job_id, rel_ms, start_ms, finish_ms, dead_ms, task.wcet_ms);
}

// Main dispatcher thread loop: releases jobs, manages ready queue, and dispatches
void Scheduler::dispatcher_loop(int64_t duration_ms) {
    base_time_ = std::chrono::steady_clock::now();
    const auto stop_time = base_time_ + std::chrono::milliseconds(duration_ms);
    for (size_t i = 0; i < tasks_.size(); ++i) next_release_[i] = base_time_;
    while (running_ && std::chrono::steady_clock::now() < stop_time) {
        const auto now = std::chrono::steady_clock::now();
        release_jobs(now);
        if (ready_queue_.empty()) {
            auto earliest = stop_time;
            for (const auto& nr : next_release_) earliest = std::min(earliest, nr);
            if (earliest > now) std::this_thread::sleep_until(earliest);
            continue;
        }
        Job top_job = ready_queue_.top();
        ready_queue_.pop();
        execute_job(top_job);
    }
}
