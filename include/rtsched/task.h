#pragma once
#include <string>
#include <functional>
#include <cstdint>

// Real-time task definition with timing parameters and optional fault injection
struct Task {
    std::string name;                          // Task identifier
    int64_t period_ms = 0;                     // Repetition period in ms
    int64_t deadline_ms = 0;                   // Relative deadline in ms (defaults to period)
    int64_t wcet_ms = 0;                       // Worst-case execution time in ms
    std::function<void()> body = nullptr;      // Executable workload function
    bool fault = false;                        // Fault injection flag (triggers overrun)

    // Constructs task; if deadline <= 0, defaults to period
    Task(std::string task_name, int64_t period, int64_t wcet,
         std::function<void()> work = nullptr, bool has_fault = false, int64_t deadline = 0)
        : name(std::move(task_name)), period_ms(period),
          deadline_ms(deadline > 0 ? deadline : period),
          wcet_ms(wcet), body(std::move(work)), fault(has_fault) {}
};
