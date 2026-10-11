#pragma once
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

// Watchdog monitor running in a dedicated thread to detect execution overruns
class Watchdog {
public:
    using Callback = std::function<void(const std::string&)>;
    explicit Watchdog(Callback recovery_callback = nullptr);
    ~Watchdog();
    void start();  // Starts the watchdog thread
    void stop();   // Stops the watchdog thread
    void notify_job_start(const std::string& task_name, int64_t wcet_ms, int multiplier = 3);
    void notify_job_finish();
    bool was_callback_fired() const;
private:
    void monitor_loop();                                          // Monitor loop thread body
    Callback callback_;                                           // User recovery callback
    std::thread thread_;                                          // Dedicated monitor thread
    std::atomic<bool> running_{false};                            // Controls thread execution
    std::atomic<bool> active_{false};                             // True while job is running
    std::atomic<bool> callback_fired_{false};                     // Prevents duplicate callbacks
    mutable std::mutex mtx_;                                      // Protects timing parameters
    std::string current_task_;                                    // Active task name
    std::chrono::steady_clock::time_point start_time_{};          // Job start timestamp
    int64_t timeout_limit_ms_ = 0;                                // Maximum allowed runtime
};
