#include "rtsched/monitor.h"
#include <algorithm>
#include <iostream>
#include <iomanip>

// Record job execution metrics
void DeadlineMonitor::record_job(const std::string& task_name, int job_id,
                                int64_t release_ms, int64_t start_ms,
                                int64_t finish_ms, int64_t deadline_ms, int64_t wcet_ms) {
    const int64_t run_time = finish_ms - start_ms;
    JobRecord rec{
        task_name, job_id, release_ms, start_ms, finish_ms, deadline_ms, wcet_ms,
        finish_ms - release_ms,                      // response_time
        finish_ms <= deadline_ms,                    // deadline met?
        std::max<int64_t>(0, start_ms - release_ms), // jitter
        std::max<int64_t>(0, run_time - wcet_ms)     // overrun
    };
    std::lock_guard<std::mutex> lock(mtx_);
    records_.push_back(rec);
}

std::vector<JobRecord> DeadlineMonitor::get_records() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return records_;
}

// Compute per-task summary statistics
std::vector<TaskSummary> DeadlineMonitor::get_summaries() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<TaskSummary> summaries;

    for (const auto& rec : records_) {
        TaskSummary* s = nullptr;
        for (auto& item : summaries) {
            if (item.task_name == rec.task_name) { s = &item; break; }
        }
        if (!s) {
            summaries.push_back(TaskSummary{rec.task_name, 0, 0, 0.0, 0, 0});
            s = &summaries.back();
        }

        s->jobs_run++;
        if (!rec.deadline_met) s->misses++;
        if (rec.overrun_ms > 0) s->overruns++;
        s->max_jitter_ms = std::max(s->max_jitter_ms, rec.jitter_ms);
        s->avg_jitter_ms += rec.jitter_ms;
    }

    for (auto& s : summaries) {
        if (s.jobs_run > 0) s.avg_jitter_ms /= s.jobs_run;
    }
    return summaries;
}

// Print formatted summary table to console
void DeadlineMonitor::print_summary() const {
    std::cout << "\n=================== TIMING MONITOR SUMMARY ===================\n"
              << std::left << std::setw(10) << "Task" << std::setw(10) << "Jobs"
              << std::setw(10) << "Misses" << std::setw(15) << "Avg Jitter(ms)"
              << std::setw(15) << "Max Jitter(ms)" << std::setw(10) << "Overruns\n"
              << "--------------------------------------------------------------\n";
    for (const auto& s : get_summaries()) {
        std::cout << std::left << std::setw(10) << s.task_name << std::setw(10) << s.jobs_run
                  << std::setw(10) << s.misses << std::setw(15) << std::fixed << std::setprecision(2)
                  << s.avg_jitter_ms << std::setw(15) << s.max_jitter_ms << std::setw(10) << s.overruns << "\n";
    }
    std::cout << "==============================================================\n\n";
}

void DeadlineMonitor::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    records_.clear();
}
