#include "rtsched/monitor.h"
#include <algorithm>
#include <iostream>
#include <iomanip>

// Record execution details of a completed job and compute metrics
void DeadlineMonitor::record_job(const std::string& task_name, int job_id,
                                int64_t release_ms, int64_t start_ms,
                                int64_t finish_ms, int64_t deadline_ms, int64_t wcet_ms) {
    JobRecord rec{task_name, job_id, release_ms, start_ms, finish_ms, deadline_ms, wcet_ms,
                  finish_ms - release_ms, finish_ms <= deadline_ms,
                  start_ms > release_ms ? start_ms - release_ms : 0,
                  (finish_ms - start_ms > wcet_ms) ? (finish_ms - start_ms - wcet_ms) : 0};
    std::lock_guard<std::mutex> lock(mtx_);
    records_.push_back(rec);
}

// Retrieve thread-safe copy of all logged records
std::vector<JobRecord> DeadlineMonitor::get_records() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return records_;
}

// Aggregate records into per-task statistical summaries
std::vector<TaskSummary> DeadlineMonitor::get_summaries() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<TaskSummary> summaries;
    for (const auto& rec : records_) {
        auto it = std::find_if(summaries.begin(), summaries.end(),
                               [&](const TaskSummary& s) { return s.task_name == rec.task_name; });
        if (it == summaries.end()) {
            summaries.push_back(TaskSummary{rec.task_name, 0, 0, 0.0, 0, 0});
            it = summaries.end() - 1;
        }
        it->jobs_run++;
        if (!rec.deadline_met) it->misses++;
        if (rec.overrun_ms > 0) it->overruns++;
        it->max_jitter_ms = std::max(it->max_jitter_ms, rec.jitter_ms);
        it->avg_jitter_ms += static_cast<double>(rec.jitter_ms);
    }
    for (auto& s : summaries) {
        if (s.jobs_run > 0) s.avg_jitter_ms /= static_cast<double>(s.jobs_run);
    }
    return summaries;
}

// Print formatted summary table to console
void DeadlineMonitor::print_summary() const {
    const auto summaries = get_summaries();
    std::cout << "\n=================== TIMING MONITOR SUMMARY ===================\n";
    std::cout << std::left << std::setw(10) << "Task" << std::setw(10) << "Jobs"
              << std::setw(10) << "Misses" << std::setw(15) << "Avg Jitter(ms)"
              << std::setw(15) << "Max Jitter(ms)" << std::setw(10) << "Overruns" << "\n";
    std::cout << "--------------------------------------------------------------\n";
    for (const auto& s : summaries) {
        std::cout << std::left << std::setw(10) << s.task_name << std::setw(10) << s.jobs_run
                  << std::setw(10) << s.misses << std::setw(15) << std::fixed << std::setprecision(2)
                  << s.avg_jitter_ms << std::setw(15) << s.max_jitter_ms << std::setw(10) << s.overruns << "\n";
    }
    std::cout << "==============================================================\n\n";
}

// Clear recorded history
void DeadlineMonitor::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    records_.clear();
}
