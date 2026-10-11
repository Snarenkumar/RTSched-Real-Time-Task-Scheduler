#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

// Detailed execution log for an individual job instance
struct JobRecord {
    std::string task_name;                     // Name of the executed task
    int job_id = 0;                            // Unique instance counter
    int64_t release_ms = 0;                    // Ideal release timestamp (ms)
    int64_t start_ms = 0;                      // Actual start timestamp (ms)
    int64_t finish_ms = 0;                     // Actual completion timestamp (ms)
    int64_t deadline_ms = 0;                   // Absolute deadline timestamp (ms)
    int64_t wcet_ms = 0;                       // Worst-case execution time parameter (ms)
    int64_t response_time_ms = 0;              // Total time from release to finish (ms)
    bool deadline_met = true;                  // True if finished on or before deadline
    int64_t jitter_ms = 0;                     // Delay between ideal release and start (ms)
    int64_t overrun_ms = 0;                    // Execution time exceeding WCET (ms)
};

// Aggregated statistical summary for each periodic task
struct TaskSummary {
    std::string task_name;                     // Task identifier
    int jobs_run = 0;                          // Total jobs dispatched
    int misses = 0;                            // Number of deadline violations
    double avg_jitter_ms = 0.0;                // Average release jitter in ms
    int64_t max_jitter_ms = 0;                 // Peak release jitter in ms
    int overruns = 0;                          // Number of jobs exceeding WCET
};

// Tracks timing metrics, computes statistics, and logs job completions
class DeadlineMonitor {
public:
    void record_job(const std::string& task_name, int job_id,
                    int64_t release_ms, int64_t start_ms,
                    int64_t finish_ms, int64_t deadline_ms, int64_t wcet_ms);
    std::vector<JobRecord> get_records() const;      // Returns copy of recorded jobs
    std::vector<TaskSummary> get_summaries() const;  // Aggregates per-task statistics
    void print_summary() const;                      // Prints formatted summary table
    void clear();                                    // Clears recorded history
private:
    mutable std::mutex mtx_;                         // Mutex protecting internal records
    std::vector<JobRecord> records_;                 // Ordered history of completed jobs
};
