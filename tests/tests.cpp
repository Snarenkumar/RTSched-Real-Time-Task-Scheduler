#include <gtest/gtest.h>
#include "rtsched/task.h"
#include "rtsched/policy.h"
#include "rtsched/monitor.h"
#include "rtsched/scheduler.h"
#include "rtsched/gantt.h"
#include <queue>
#include <vector>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>

// Test Task construction with default deadline matching period
TEST(TaskTest, DefaultDeadlineToPeriod) {
    Task t("T1", 50, 10);
    EXPECT_EQ(t.name, "T1");
    EXPECT_EQ(t.period_ms, 50);
    EXPECT_EQ(t.deadline_ms, 50);
    EXPECT_EQ(t.wcet_ms, 10);
    EXPECT_FALSE(t.fault);
}

// Test Task construction with explicit deadline and fault flag
TEST(TaskTest, ExplicitDeadlineAndFault) {
    Task t("T2", 100, 20, nullptr, true, 80);
    EXPECT_EQ(t.name, "T2");
    EXPECT_EQ(t.period_ms, 100);
    EXPECT_EQ(t.deadline_ms, 80);
    EXPECT_EQ(t.wcet_ms, 20);
    EXPECT_TRUE(t.fault);
}

// Test processor utilization calculation on empty task set
TEST(AdmissionTest, UtilizationEmpty) {
    std::vector<Task> tasks;
    EXPECT_DOUBLE_EQ(calculate_utilization(tasks), 0.0);
}

// Test processor utilization calculation on multiple tasks
TEST(AdmissionTest, UtilizationSum) {
    std::vector<Task> tasks = {
        Task("T1", 50, 10),   // U = 10 / 50 = 0.20
        Task("T2", 100, 20),  // U = 20 / 100 = 0.20
        Task("T3", 200, 40)   // U = 40 / 200 = 0.20
    };
    EXPECT_NEAR(calculate_utilization(tasks), 0.60, 1e-6);
}

// Test Liu and Layland RM bound for n = 0
TEST(AdmissionTest, RMBoundZero) {
    EXPECT_DOUBLE_EQ(rm_bound(0), 0.0);
}

// Test Liu and Layland RM bound for n = 1
TEST(AdmissionTest, RMBoundN1) {
    EXPECT_NEAR(rm_bound(1), 1.0, 1e-6);
}

// Test Liu and Layland RM bound for n = 2
TEST(AdmissionTest, RMBoundN2) {
    EXPECT_NEAR(rm_bound(2), 0.828427, 1e-5);
}

// Test Liu and Layland RM bound for n = 3
TEST(AdmissionTest, RMBoundN3) {
    EXPECT_NEAR(rm_bound(3), 0.779763, 1e-5);
}

// Test Liu and Layland RM bound for n = 10
TEST(AdmissionTest, RMBoundN10) {
    EXPECT_NEAR(rm_bound(10), 0.717735, 1e-5);
}

// Test admission control accepts RM task set when U <= bound
TEST(AdmissionTest, RMAcceptWhenUnderBound) {
    std::vector<Task> tasks = {
        Task("T1", 50, 10),
        Task("T2", 100, 20),
        Task("T3", 200, 40)
    };
    EXPECT_TRUE(check_admission(tasks, Policy::RM, false));
}

// Test admission control rejects RM task set when U > bound
TEST(AdmissionTest, RMRejectWhenOverBound) {
    std::vector<Task> tasks = {
        Task("T1", 100, 30),
        Task("T2", 100, 30),
        Task("T3", 100, 25)
    };
    EXPECT_FALSE(check_admission(tasks, Policy::RM, false));
}

// Test admission control accepts EDF task set when U <= 1.0
TEST(AdmissionTest, EDFAcceptWhenUnderOne) {
    std::vector<Task> tasks = {
        Task("T1", 100, 30),
        Task("T2", 100, 30),
        Task("T3", 100, 25)
    };
    EXPECT_TRUE(check_admission(tasks, Policy::EDF, false));
}

// Test admission control rejects EDF task set when U > 1.0
TEST(AdmissionTest, EDFRejectWhenOverOne) {
    std::vector<Task> tasks = {
        Task("T1", 100, 50),
        Task("T2", 100, 40),
        Task("T3", 100, 20)
    };
    EXPECT_FALSE(check_admission(tasks, Policy::EDF, false));
}

// Test RM priority queue pops shorter periods first
TEST(PriorityQueueTest, RMPriorityShorterPeriodFirst) {
    JobComparator cmp{Policy::RM};
    std::priority_queue<Job, std::vector<Job>, JobComparator> pq(cmp);

    const auto now = std::chrono::steady_clock::now();
    Job j1{0, 1, now, now + std::chrono::milliseconds(200), 200, 40};
    Job j2{1, 1, now, now + std::chrono::milliseconds(50),  50,  10};
    Job j3{2, 1, now, now + std::chrono::milliseconds(100), 100, 20};

    pq.push(j1);
    pq.push(j2);
    pq.push(j3);

    EXPECT_EQ(pq.top().period_ms, 50);
    pq.pop();
    EXPECT_EQ(pq.top().period_ms, 100);
    pq.pop();
    EXPECT_EQ(pq.top().period_ms, 200);
    pq.pop();
}

// Test RM priority queue tie-breaking with earlier release time
TEST(PriorityQueueTest, RMTieBreakEarlierRelease) {
    JobComparator cmp{Policy::RM};
    std::priority_queue<Job, std::vector<Job>, JobComparator> pq(cmp);

    const auto now = std::chrono::steady_clock::now();
    Job early{0, 1, now, now + std::chrono::milliseconds(100), 100, 20};
    Job late{1, 1, now + std::chrono::milliseconds(10), now + std::chrono::milliseconds(110), 100, 20};

    pq.push(late);
    pq.push(early);

    EXPECT_EQ(pq.top().task_index, 0u);
    pq.pop();
    EXPECT_EQ(pq.top().task_index, 1u);
    pq.pop();
}

// Test EDF priority queue pops earlier absolute deadline first
TEST(PriorityQueueTest, EDFPriorityEarlierDeadlineFirst) {
    JobComparator cmp{Policy::EDF};
    std::priority_queue<Job, std::vector<Job>, JobComparator> pq(cmp);

    const auto now = std::chrono::steady_clock::now();
    Job j1{0, 1, now, now + std::chrono::milliseconds(300), 100, 20};
    Job j2{1, 1, now, now + std::chrono::milliseconds(80),  100, 20};
    Job j3{2, 1, now, now + std::chrono::milliseconds(150), 100, 20};

    pq.push(j1);
    pq.push(j2);
    pq.push(j3);

    EXPECT_EQ(pq.top().task_index, 1u);
    pq.pop();
    EXPECT_EQ(pq.top().task_index, 2u);
    pq.pop();
    EXPECT_EQ(pq.top().task_index, 0u);
    pq.pop();
}

// Test EDF priority queue tie-breaking with earlier release time
TEST(PriorityQueueTest, EDFTieBreakEarlierRelease) {
    JobComparator cmp{Policy::EDF};
    std::priority_queue<Job, std::vector<Job>, JobComparator> pq(cmp);

    const auto now = std::chrono::steady_clock::now();
    const auto same_deadline = now + std::chrono::milliseconds(200);
    Job early{0, 1, now, same_deadline, 100, 20};
    Job late{1, 1, now + std::chrono::milliseconds(20), same_deadline, 100, 20};

    pq.push(late);
    pq.push(early);

    EXPECT_EQ(pq.top().task_index, 0u);
    pq.pop();
    EXPECT_EQ(pq.top().task_index, 1u);
    pq.pop();
}

// Test DeadlineMonitor metric calculations for a normal on-time job
TEST(MonitorTest, NormalJobMetrics) {
    DeadlineMonitor mon;
    mon.record_job("T1", 1, 100, 102, 112, 150, 10);

    const auto recs = mon.get_records();
    ASSERT_EQ(recs.size(), 1u);
    EXPECT_EQ(recs[0].task_name, "T1");
    EXPECT_EQ(recs[0].jitter_ms, 2);
    EXPECT_EQ(recs[0].response_time_ms, 12);
    EXPECT_EQ(recs[0].overrun_ms, 0);
    EXPECT_TRUE(recs[0].deadline_met);
}

// Test DeadlineMonitor detects overruns and missed deadlines
TEST(MonitorTest, OverrunAndMissMetrics) {
    DeadlineMonitor mon;
    mon.record_job("T1", 1, 100, 110, 160, 150, 20);

    const auto recs = mon.get_records();
    ASSERT_EQ(recs.size(), 1u);
    EXPECT_EQ(recs[0].jitter_ms, 10);
    EXPECT_EQ(recs[0].response_time_ms, 60);
    EXPECT_EQ(recs[0].overrun_ms, 30);
    EXPECT_FALSE(recs[0].deadline_met);
}

// Test DeadlineMonitor statistical summary aggregation across tasks
TEST(MonitorTest, SummaryAggregation) {
    DeadlineMonitor mon;
    mon.record_job("T1", 1, 100, 102, 112, 150, 10);
    mon.record_job("T1", 2, 200, 208, 260, 250, 20);
    mon.record_job("T2", 1, 100, 100, 120, 200, 20);

    const auto summaries = mon.get_summaries();
    ASSERT_EQ(summaries.size(), 2u);

    EXPECT_EQ(summaries[0].task_name, "T1");
    EXPECT_EQ(summaries[0].jobs_run, 2);
    EXPECT_EQ(summaries[0].misses, 1);
    EXPECT_EQ(summaries[0].overruns, 1);
    EXPECT_NEAR(summaries[0].avg_jitter_ms, 5.0, 1e-6);
    EXPECT_EQ(summaries[0].max_jitter_ms, 8);

    EXPECT_EQ(summaries[1].task_name, "T2");
    EXPECT_EQ(summaries[1].jobs_run, 1);
    EXPECT_EQ(summaries[1].misses, 0);
    EXPECT_EQ(summaries[1].overruns, 0);
    EXPECT_DOUBLE_EQ(summaries[1].avg_jitter_ms, 0.0);
    EXPECT_EQ(summaries[1].max_jitter_ms, 0);
}

// Test clearing DeadlineMonitor resets history
TEST(MonitorTest, ClearResetsRecords) {
    DeadlineMonitor mon;
    mon.record_job("T1", 1, 0, 0, 10, 50, 10);
    EXPECT_EQ(mon.get_records().size(), 1u);
    mon.clear();
    EXPECT_EQ(mon.get_records().size(), 0u);
    EXPECT_EQ(mon.get_summaries().size(), 0u);
}

// Integration Test 1: Run short schedule and verify each task dispatches at least once
TEST(IntegrationTest, SchedulerExecution) {
    std::vector<Task> tasks = {
        Task("T1", 50, 5),
        Task("T2", 100, 5)
    };
    Scheduler sched(tasks, Policy::RM);
    sched.run_for(250);

    const auto summaries = sched.get_monitor().get_summaries();
    ASSERT_GE(summaries.size(), 2u);
    EXPECT_GE(summaries[0].jobs_run, 3);
    EXPECT_GE(summaries[1].jobs_run, 1);
}

// Integration Test 2: Injected fault triggers watchdog recovery callback
TEST(IntegrationTest, WatchdogFaultRecovery) {
    std::atomic<bool> callback_called{false};
    std::string recovered_task;
    std::mutex cb_mtx;
    auto recovery = [&](const std::string& name) {
        std::lock_guard<std::mutex> lock(cb_mtx);
        callback_called = true;
        recovered_task = name;
    };

    std::vector<Task> tasks = {
        Task("FaultyTask", 50, 15, nullptr, true)
    };
    Scheduler sched(tasks, Policy::RM, recovery);
    sched.run_for(250);

    EXPECT_TRUE(callback_called.load());
    {
        std::lock_guard<std::mutex> lock(cb_mtx);
        EXPECT_EQ(recovered_task, "FaultyTask");
    }
    const auto summaries = sched.get_monitor().get_summaries();
    ASSERT_FALSE(summaries.empty());
    EXPECT_GT(summaries[0].overruns, 0);
}

// Test Gantt text format output with fixed deterministic records
TEST(GanttTest, FormatTextGantt) {
    std::vector<JobRecord> records = {
        JobRecord{"T1", 1, 0, 0, 10, 20, 10, 10, true, 0, 0},
        JobRecord{"T2", 1, 0, 10, 20, 20, 10, 20, true, 10, 0}
    };
    std::vector<std::string> names = {"T1", "T2"};
    const std::string text = format_text_gantt(records, names, 20, 5);

    EXPECT_TRUE(text.find("T1      |##..|") != std::string::npos);
    EXPECT_TRUE(text.find("T2      |..##|") != std::string::npos);
}

// Test exporting timeline to CSV file format
TEST(GanttTest, ExportTimelineCSV) {
    std::vector<JobRecord> records = {
        JobRecord{"T1", 1, 0, 0, 10, 20, 10, 10, true, 0, 0},
        JobRecord{"T2", 1, 0, 10, 20, 20, 10, 20, true, 10, 0}
    };
    const std::string test_csv = "test_timeline.csv";
    EXPECT_TRUE(export_timeline_csv(test_csv, records));

    std::ifstream file(test_csv);
    ASSERT_TRUE(file.is_open());
    std::string header, line1, line2;
    std::getline(file, header);
    std::getline(file, line1);
    std::getline(file, line2);

    EXPECT_EQ(header, "task,start_ms,end_ms");
    EXPECT_EQ(line1, "T1,0,10");
    EXPECT_EQ(line2, "T2,10,20");
    file.close();
    std::remove(test_csv.c_str());
}
