#include "rtsched/watchdog.h"

// Initialize watchdog with optional user callback
Watchdog::Watchdog(Callback recovery_callback) : callback_(std::move(recovery_callback)) {}
Watchdog::~Watchdog() { stop(); } // Ensure monitor thread is stopped upon destruction

// Start dedicated background monitor thread
void Watchdog::start() {
    running_ = true;
    thread_ = std::thread(&Watchdog::monitor_loop, this);
}

// Terminate watchdog thread safely
void Watchdog::stop() {
    if (running_.exchange(false) && thread_.joinable()) thread_.join();
}

// Arm watchdog for a newly started job
void Watchdog::notify_job_start(const std::string& task_name, int64_t wcet_ms, int multiplier) {
    std::lock_guard<std::mutex> lock(mtx_);
    current_task_ = task_name;
    timeout_limit_ms_ = wcet_ms * multiplier;
    start_time_ = std::chrono::steady_clock::now();
    callback_fired_ = false;
    active_ = true;
}

void Watchdog::notify_job_finish() { active_ = false; } // Disarm watchdog
bool Watchdog::was_callback_fired() const { return callback_fired_.load(); }

// Monitor loop checking job run-times against safety threshold
void Watchdog::monitor_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if (!active_ || callback_fired_) continue;
        std::string task_to_recover;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time_).count();
            if (elapsed >= timeout_limit_ms_ && !callback_fired_) {
                callback_fired_ = true;
                task_to_recover = current_task_;
            }
        }
        if (!task_to_recover.empty() && callback_) callback_(task_to_recover);
    }
}
