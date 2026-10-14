#include "rtsched/scheduler.h"
#include <algorithm>

// Spin CPU for ms milliseconds (simulates real work)
void Scheduler::spin_busy(int64_t ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {}
}

Scheduler::Scheduler(std::vector<Task> tasks, Policy policy, Watchdog::Callback recovery_cb)
    : tasks_(std::move(tasks)), policy_(policy), comparator_{policy}, ready_queue_(comparator_),
      watchdog_(std::move(recovery_cb)), next_release_(tasks_.size()), job_ids_(tasks_.size(), 1) {}

Scheduler::~Scheduler() {
    running_ = false;
    if (dispatcher_thread_.joinable()) dispatcher_thread_.join();
}

// Start scheduler execution for duration_ms
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

// Check if any periodic tasks are due to release new jobs
void Scheduler::release_jobs(const std::chrono::steady_clock::time_point& now) {
    for (size_t i = 0; i < tasks_.size(); ++i) {
        while (next_release_[i] <= now) {
            auto deadline = next_release_[i] + std::chrono::milliseconds(tasks_[i].deadline_ms);
            ready_queue_.push(Job{i, job_ids_[i]++, next_release_[i], deadline, tasks_[i].period_ms, tasks_[i].wcet_ms});
            next_release_[i] += std::chrono::milliseconds(tasks_[i].period_ms);
        }
    }
}

// Run the top job from the queue and record timing
void Scheduler::execute_job(const Job& job) {
    const auto& task = tasks_[job.task_index];
    const auto start_tp = std::chrono::steady_clock::now();

    // 1. Arm watchdog timer
    watchdog_.notify_job_start(task.name, task.wcet_ms);

    // 2. Execute workload (simulate fault by spinning 5x longer on job 3)
    if (task.body) task.body();
    else if (task.fault && job.job_id == 3) spin_busy(task.wcet_ms * 5);
    else spin_busy(task.wcet_ms);

    // 3. Disarm watchdog timer
    watchdog_.notify_job_finish();
    const auto finish_tp = std::chrono::steady_clock::now();

    // 4. Record timestamps relative to base time (in ms)
    auto to_ms = [&](const std::chrono::steady_clock::time_point& tp) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp - base_time_).count();
    };
    monitor_.record_job(task.name, job.job_id, to_ms(job.release_time), to_ms(start_tp),
                       to_ms(finish_tp), to_ms(job.absolute_deadline), task.wcet_ms);
}

// Main background scheduler loop
void Scheduler::dispatcher_loop(int64_t duration_ms) {
    base_time_ = std::chrono::steady_clock::now();
    const auto stop_time = base_time_ + std::chrono::milliseconds(duration_ms);
    for (size_t i = 0; i < tasks_.size(); ++i) next_release_[i] = base_time_;

    while (running_ && std::chrono::steady_clock::now() < stop_time) {
        const auto now = std::chrono::steady_clock::now();
        release_jobs(now);

        if (ready_queue_.empty()) {
            // Find earliest next task arrival and sleep until then
            auto earliest = stop_time;
            for (const auto& nr : next_release_) earliest = std::min(earliest, nr);
            if (earliest > now) std::this_thread::sleep_until(earliest);
            continue;
        }

        // Take highest-priority job and execute it
        Job top_job = ready_queue_.top();
        ready_queue_.pop();
        execute_job(top_job);
    }
}
