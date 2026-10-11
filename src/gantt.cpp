#include "rtsched/gantt.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

// Export execution intervals to CSV format
bool export_timeline_csv(const std::string& filepath, const std::vector<JobRecord>& records) {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    out << "task,start_ms,end_ms\n";
    for (const auto& rec : records) {
        out << rec.task_name << "," << rec.start_ms << "," << rec.finish_ms << "\n";
    }
    return true;
}

// Generate ASCII text Gantt chart showing task execution over 5ms slots
std::string format_text_gantt(const std::vector<JobRecord>& records,
                              const std::vector<std::string>& task_names,
                              int64_t total_duration_ms, int64_t slot_duration_ms) {
    std::ostringstream ss;
    const int num_slots = static_cast<int>(total_duration_ms / slot_duration_ms);
    for (const auto& name : task_names) {
        ss << std::left << std::setw(8) << name << "|";
        for (int s = 0; s < num_slots; ++s) {
            const int64_t slot_start = s * slot_duration_ms;
            const int64_t slot_end = slot_start + slot_duration_ms;
            bool active = false;
            for (const auto& r : records) {
                if (r.task_name == name && std::max(slot_start, r.start_ms) < std::min(slot_end, r.finish_ms)) {
                    active = true;
                    break;
                }
            }
            ss << (active ? '#' : '.');
        }
        ss << "|\n";
    }
    return ss.str();
}

// Print Gantt chart directly to console
void print_text_gantt(const std::vector<JobRecord>& records,
                      const std::vector<std::string>& task_names,
                      int64_t total_duration_ms, int64_t slot_duration_ms) {
    std::cout << "\nExecution Gantt Chart (1 char = " << slot_duration_ms << " ms):\n";
    std::cout << format_text_gantt(records, task_names, total_duration_ms, slot_duration_ms) << "\n";
}
