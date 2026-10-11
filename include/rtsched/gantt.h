#pragma once
#include "monitor.h"
#include <string>
#include <vector>

// Exports execution records to CSV format with header: task,start_ms,end_ms
bool export_timeline_csv(const std::string& filepath, const std::vector<JobRecord>& records);

// Formats a text-based Gantt chart with 5ms time slices per character
std::string format_text_gantt(const std::vector<JobRecord>& records,
                              const std::vector<std::string>& task_names,
                              int64_t total_duration_ms, int64_t slot_duration_ms = 5);

// Prints the text Gantt chart directly to standard output
void print_text_gantt(const std::vector<JobRecord>& records,
                      const std::vector<std::string>& task_names,
                      int64_t total_duration_ms, int64_t slot_duration_ms = 5);
