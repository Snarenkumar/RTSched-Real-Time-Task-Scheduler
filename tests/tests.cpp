// ==============================================================================
// RTSched Automated Test Suite (GoogleTest)
// ==============================================================================
// NOTE FOR BEGINNERS:
// - Does this touch or change your Windows OS? NO!
//   This is a standalone C++ program. It creates its own simulated tasks and 
//   runs them on normal C++ threads (`std::thread`) inside this process.
// - What are TEST(Suite, Name)?
//   They are GoogleTest test cases. GoogleTest calls each test one by one.
// - What are EXPECT_EQ, EXPECT_TRUE, etc.?
//   They are "asserts" (verification checks). If the condition is false,
//   the test fails with an error message. If all pass, GoogleTest prints [PASSED].
// ==============================================================================

#include <gtest/gtest.h>
#include "rtsched/task.h"       // Defines struct Task (blueprint of a real-time task)
#include "rtsched/policy.h"     // Admission math (RM bound, utilization) and JobComparator
#include "rtsched/monitor.h"    // DeadlineMonitor (records jitter, misses, overruns)
#include "rtsched/scheduler.h"  // Scheduler engine (queues jobs, dispatches threads)
#include "rtsched/gantt.h"      // Gantt chart renderer & CSV exporter
#include <queue>
#include <vector>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>

// ==============================================================================
// TEST SUITE 1: TaskTest
// Testing the 'Task' struct: Does it hold and set our parameters correctly?
// ==============================================================================

// TEST 1.1: If we do NOT specify a deadline, it should automatically equal the period.
TEST(TaskTest, DefaultDeadlineToPeriod) {
    // OBJECT CREATION:
    // Create a Task object 't' with: Name="T1", Period=50ms, WCET(runtime)=10ms.
    Task t("T1", 50, 10);

    // VERIFICATION:
    EXPECT_EQ(t.name, "T1");        // Name must match "T1"
    EXPECT_EQ(t.period_ms, 50);     // Period must be 50ms
    EXPECT_EQ(t.deadline_ms, 50);   // Deadline wasn't given, so it should default to 50ms!
    EXPECT_EQ(t.wcet_ms, 10);       // Execution time must be 10ms
    EXPECT_FALSE(t.fault);          // No fault injected by default
}

// TEST 1.2: If we explicitly give a custom deadline (80ms) and fault=true.
TEST(TaskTest, ExplicitDeadlineAndFault) {
    // OBJECT CREATION:
    // Task(name, period, wcet, body_function, fault_flag, custom_deadline)
    Task t("T2", 100, 20, nullptr, true, 80);

    // VERIFICATION:
    EXPECT_EQ(t.name, "T2");
    EXPECT_EQ(t.period_ms, 100);
    EXPECT_EQ(t.deadline_ms, 80);   // Custom deadline honored!
    EXPECT_EQ(t.wcet_ms, 20);
    EXPECT_TRUE(t.fault);           // Fault flag is enabled!
}

// ==============================================================================
// TEST SUITE 2: AdmissionTest
// Testing the admission control math before any task is allowed to run.
// Formula: Utilization U = sum(Execution_Time / Period)
// ==============================================================================

// TEST 2.1: Empty task list should have 0% utilization (0.0).
TEST(AdmissionTest, UtilizationEmpty) {
    std::vector<Task> tasks; // Empty vector
    EXPECT_DOUBLE_EQ(calculate_utilization(tasks), 0.0);
}

// TEST 2.2: Multiple tasks utilization sum.
// T1: 10ms / 50ms  = 0.20
// T2: 20ms / 100ms = 0.20
// T3: 40ms / 200ms = 0.20
// Total U = 0.20 + 0.20 + 0.20 = 0.60 (60% CPU workload)
TEST(AdmissionTest, UtilizationSum) {
    std::vector<Task> tasks = {
        Task("T1", 50, 10),
        Task("T2", 100, 20),
        Task("T3", 200, 40)
    };
    EXPECT_NEAR(calculate_utilization(tasks), 0.60, 1e-6);
}

// TEST 2.3 - 2.7: Testing the Liu & Layland RM Bound formula:
// Bound(n) = n * (2^(1/n) - 1)
TEST(AdmissionTest, RMBoundZero) {
    EXPECT_DOUBLE_EQ(rm_bound(0), 0.0); // 0 tasks = 0 bound
}

TEST(AdmissionTest, RMBoundN1) {
    // For n=1 task: 1 * (2^1 - 1) = 1.0 (100% capacity)
    EXPECT_NEAR(rm_bound(1), 1.0, 1e-6);
}

TEST(AdmissionTest, RMBoundN2) {
    // For n=2 tasks: 2 * (2^(1/2) - 1) = 2 * (1.4142 - 1) ~ 0.8284 (82.8%)
    EXPECT_NEAR(rm_bound(2), 0.828427, 1e-5);
}

TEST(AdmissionTest, RMBoundN3) {
    // For n=3 tasks: 3 * (2^(1/3) - 1) ~ 0.7798 (78%)
    EXPECT_NEAR(rm_bound(3), 0.779763, 1e-5);
}

TEST(AdmissionTest, RMBoundN10) {
    // For n=10 tasks: ~ 0.7177 (71.8%)
    EXPECT_NEAR(rm_bound(10), 0.717735, 1e-5);
}

// TEST 2.8: RM Policy accepts task set when U (0.60) <= Bound (0.78)
TEST(AdmissionTest, RMAcceptWhenUnderBound) {
    std::vector<Task> tasks = {
        Task("T1", 50, 10),
        Task("T2", 100, 20),
        Task("T3", 200, 40)
    };
    // check_admission returns true -> accepted
    EXPECT_TRUE(check_admission(tasks, Policy::RM, false));
}

// TEST 2.9: RM Policy rejects task set when U exceeds bound.
// T1: 30/100, T2: 30/100, T3: 25/100 -> U = 0.85
// For n=3, RM bound is 0.78. Since 0.85 > 0.78, RM must reject!
TEST(AdmissionTest, RMRejectWhenOverBound) {
    std::vector<Task> tasks = {
        Task("T1", 100, 30),
        Task("T2", 100, 30),
        Task("T3", 100, 25)
    };
    EXPECT_FALSE(check_admission(tasks, Policy::RM, false));
}

// TEST 2.10: EDF accepts the exact same task set (U=0.85) because EDF bound is 1.0 (100%)!
TEST(AdmissionTest, EDFAcceptWhenUnderOne) {
    std::vector<Task> tasks = {
        Task("T1", 100, 30),
        Task("T2", 100, 30),
        Task("T3", 100, 25)
    };
    // Since 0.85 <= 1.0, EDF accepts!
    EXPECT_TRUE(check_admission(tasks, Policy::EDF, false));
}

// TEST 2.11: EDF rejects when U > 1.0 (over 100% capacity)
// T1: 50/100, T2: 40/100, T3: 20/100 -> U = 1.10 (110% CPU load - impossible!)
TEST(AdmissionTest, EDFRejectWhenOverOne) {
    std::vector<Task> tasks = {
        Task("T1", 100, 50),
        Task("T2", 100, 40),
        Task("T3", 100, 20)
    };
    EXPECT_FALSE(check_admission(tasks, Policy::EDF, false));
}

// ==============================================================================
// TEST SUITE 3: PriorityQueueTest
// Testing the sorting logic: When multiple jobs arrive, who gets the CPU first?
// ==============================================================================

// TEST 3.1: Under RM (Rate-Monotonic), shorter period = higher priority.
// Periods: 50ms, 100ms, 200ms -> Must pop 50ms first, then 100ms, then 200ms.
TEST(PriorityQueueTest, RMPriorityShorterPeriodFirst) {
    JobComparator cmp{Policy::RM}; // Functor comparator using RM rule
    std::priority_queue<Job, std::vector<Job>, JobComparator> pq(cmp);

    const auto now = std::chrono::steady_clock::now();
    Job j1{0, 1, now, now + std::chrono::milliseconds(200), 200, 40}; // Period = 200ms
    Job j2{1, 1, now, now + std::chrono::milliseconds(50),  50,  10}; // Period = 50ms
    Job j3{2, 1, now, now + std::chrono::milliseconds(100), 100, 20}; // Period = 100ms

    // Push into queue in random order
    pq.push(j1);
    pq.push(j2);
    pq.push(j3);

    // Verify correct sorting order
    EXPECT_EQ(pq.top().period_ms, 50);  // 1st: 50ms
    pq.pop();
    EXPECT_EQ(pq.top().period_ms, 100); // 2nd: 100ms
    pq.pop();
    EXPECT_EQ(pq.top().period_ms, 200); // 3rd: 200ms
    pq.pop();
}

// TEST 3.2: RM tie-break: If two jobs have the SAME period, who was released earlier wins.
TEST(PriorityQueueTest, RMTieBreakEarlierRelease) {
    JobComparator cmp{Policy::RM};
    std::priority_queue<Job, std::vector<Job>, JobComparator> pq(cmp);

    const auto now = std::chrono::steady_clock::now();
    // Both jobs have period = 100ms, but 'early' arrived 10ms sooner than 'late'
    Job early{0, 1, now, now + std::chrono::milliseconds(100), 100, 20};
    Job late{1, 1, now + std::chrono::milliseconds(10), now + std::chrono::milliseconds(110), 100, 20};

    pq.push(late);
    pq.push(early);

    // 'early' (task_index 0) must be popped first!
    EXPECT_EQ(pq.top().task_index, 0u);
    pq.pop();
    EXPECT_EQ(pq.top().task_index, 1u);
    pq.pop();
}

// TEST 3.3: Under EDF (Earliest Deadline First), closest deadline = highest priority.
// Deadlines: 80ms, 150ms, 300ms -> Must pop 80ms, then 150ms, then 300ms.
TEST(PriorityQueueTest, EDFPriorityEarlierDeadlineFirst) {
    JobComparator cmp{Policy::EDF};
    std::priority_queue<Job, std::vector<Job>, JobComparator> pq(cmp);

    const auto now = std::chrono::steady_clock::now();
    Job j1{0, 1, now, now + std::chrono::milliseconds(300), 100, 20}; // Deadline in 300ms
    Job j2{1, 1, now, now + std::chrono::milliseconds(80),  100, 20}; // Deadline in 80ms (closest!)
    Job j3{2, 1, now, now + std::chrono::milliseconds(150), 100, 20}; // Deadline in 150ms

    pq.push(j1);
    pq.push(j2);
    pq.push(j3);

    EXPECT_EQ(pq.top().task_index, 1u); // j2 (80ms)
    pq.pop();
    EXPECT_EQ(pq.top().task_index, 2u); // j3 (150ms)
    pq.pop();
    EXPECT_EQ(pq.top().task_index, 0u); // j1 (300ms)
    pq.pop();
}

// TEST 3.4: EDF tie-break: If deadlines are identical, earlier release time wins.
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

// ==============================================================================
// TEST SUITE 4: MonitorTest
// Testing the DeadlineMonitor: Does it calculate jitter, response time, and misses correctly?
// ==============================================================================

// TEST 4.1: Record a healthy job that finished on time.
// Released at 100ms, Started at 102ms (Jitter = 102 - 100 = 2ms)
// Finished at 112ms, Deadline was 150ms (Finished before deadline? YES! Response time = 12ms)
TEST(MonitorTest, NormalJobMetrics) {
    DeadlineMonitor mon; // Object of DeadlineMonitor class
    mon.record_job("T1", 1, 100, 102, 112, 150, 10);

    const auto recs = mon.get_records();
    ASSERT_EQ(recs.size(), 1u);
    EXPECT_EQ(recs[0].task_name, "T1");
    EXPECT_EQ(recs[0].jitter_ms, 2);              // Waited 2ms
    EXPECT_EQ(recs[0].response_time_ms, 12);       // Took 12ms total
    EXPECT_EQ(recs[0].overrun_ms, 0);              // Ran for 10ms <= WCET 10ms (no overrun)
    EXPECT_TRUE(recs[0].deadline_met);             // Met deadline!
}

// TEST 4.2: Record a job that took too long (overrun) and missed its deadline.
// Released: 100ms, Started: 110ms (Jitter = 10ms)
// Finished: 160ms, Deadline: 150ms -> MISSED DEADLINE! (160 > 150)
// WCET promised: 20ms. Actual run: 160 - 110 = 50ms. Overrun = 50 - 20 = 30ms!
TEST(MonitorTest, OverrunAndMissMetrics) {
    DeadlineMonitor mon;
    mon.record_job("T1", 1, 100, 110, 160, 150, 20);

    const auto recs = mon.get_records();
    ASSERT_EQ(recs.size(), 1u);
    EXPECT_EQ(recs[0].jitter_ms, 10);
    EXPECT_EQ(recs[0].response_time_ms, 60);
    EXPECT_EQ(recs[0].overrun_ms, 30);
    EXPECT_FALSE(recs[0].deadline_met); // Caught as missed deadline!
}

// TEST 4.3: Statistical aggregation across multiple tasks and jobs.
// Verifies that average jitter, total misses, and total runs are calculated accurately.
TEST(MonitorTest, SummaryAggregation) {
    DeadlineMonitor mon;
    // T1 Job 1: jitter 2ms, deadline met
    mon.record_job("T1", 1, 100, 102, 112, 150, 10);
    // T1 Job 2: jitter 8ms, missed deadline, overrun
    mon.record_job("T1", 2, 200, 208, 260, 250, 20);
    // T2 Job 1: jitter 0ms, deadline met
    mon.record_job("T2", 1, 100, 100, 120, 200, 20);

    const auto summaries = mon.get_summaries();
    ASSERT_EQ(summaries.size(), 2u);

    // Check summary for T1:
    EXPECT_EQ(summaries[0].task_name, "T1");
    EXPECT_EQ(summaries[0].jobs_run, 2);
    EXPECT_EQ(summaries[0].misses, 1);
    EXPECT_EQ(summaries[0].overruns, 1);
    EXPECT_NEAR(summaries[0].avg_jitter_ms, 5.0, 1e-6); // (2 + 8) / 2 = 5.0ms
    EXPECT_EQ(summaries[0].max_jitter_ms, 8);

    // Check summary for T2:
    EXPECT_EQ(summaries[1].task_name, "T2");
    EXPECT_EQ(summaries[1].jobs_run, 1);
    EXPECT_EQ(summaries[1].misses, 0);
    EXPECT_EQ(summaries[1].overruns, 0);
    EXPECT_DOUBLE_EQ(summaries[1].avg_jitter_ms, 0.0);
    EXPECT_EQ(summaries[1].max_jitter_ms, 0);
}

// TEST 4.4: Does mon.clear() wipe all history clean?
TEST(MonitorTest, ClearResetsRecords) {
    DeadlineMonitor mon;
    mon.record_job("T1", 1, 0, 0, 10, 50, 10);
    EXPECT_EQ(mon.get_records().size(), 1u);
    mon.clear(); // Wipe
    EXPECT_EQ(mon.get_records().size(), 0u);
    EXPECT_EQ(mon.get_summaries().size(), 0u);
}

// ==============================================================================
// TEST SUITE 5: IntegrationTest
// Testing the full system: Tasks -> Scheduler engine -> Threads -> Watchdog
// ==============================================================================

// TEST 5.1: Run a live 250ms schedule on a real C++ background thread.
// Verifies that the dispatcher loop wakes up, pops jobs, and executes them in real time.
TEST(IntegrationTest, SchedulerExecution) {
    std::vector<Task> tasks = {
        Task("T1", 50, 5),   // repeats every 50ms
        Task("T2", 100, 5)   // repeats every 100ms
    };
    Scheduler sched(tasks, Policy::RM);
    sched.run_for(250); // Run in real time for 250ms

    const auto summaries = sched.get_monitor().get_summaries();
    ASSERT_GE(summaries.size(), 2u);
    // In 250ms, T1 (50ms period) should execute at least 3-5 times
    EXPECT_GE(summaries[0].jobs_run, 3);
    // In 250ms, T2 (100ms period) should execute at least 1-2 times
    EXPECT_GE(summaries[1].jobs_run, 1);
}

// TEST 5.2: Fault Injection & Watchdog Timer Recovery.
// What happens if a task freezes or loops infinitely?
// A separate Watchdog thread detects the overrun and invokes our recovery callback!
TEST(IntegrationTest, WatchdogFaultRecovery) {
    std::atomic<bool> callback_called{false};
    std::string recovered_task;
    std::mutex cb_mtx;

    // This callback function is our emergency recovery handler:
    auto recovery = [&](const std::string& name) {
        std::lock_guard<std::mutex> lock(cb_mtx);
        callback_called = true;
        recovered_task = name;
    };

    // 'FaultyTask' has fault=true, which forces job 3 to spin 5x longer than allowed!
    std::vector<Task> tasks = {
        Task("FaultyTask", 50, 15, nullptr, true)
    };
    Scheduler sched(tasks, Policy::RM, recovery);
    sched.run_for(250);

    // Did the watchdog wake up and trigger the emergency callback? YES!
    EXPECT_TRUE(callback_called.load());
    {
        std::lock_guard<std::mutex> lock(cb_mtx);
        EXPECT_EQ(recovered_task, "FaultyTask"); // Watchdog identified the faulty task!
    }
    const auto summaries = sched.get_monitor().get_summaries();
    ASSERT_FALSE(summaries.empty());
    EXPECT_GT(summaries[0].overruns, 0); // Recorded as an overrun
}

// ==============================================================================
// TEST SUITE 6: GanttTest
// Testing visual timeline rendering and CSV file export.
// ==============================================================================

// TEST 6.1: ASCII Gantt chart string formatting.
// T1 runs from 0 to 10ms (two 5ms slots -> '##')
// T2 runs from 10 to 20ms (two 5ms slots -> '##')
TEST(GanttTest, FormatTextGantt) {
    std::vector<JobRecord> records = {
        JobRecord{"T1", 1, 0, 0, 10, 20, 10, 10, true, 0, 0},
        JobRecord{"T2", 1, 0, 10, 20, 20, 10, 20, true, 10, 0}
    };
    std::vector<std::string> names = {"T1", "T2"};
    const std::string text = format_text_gantt(records, names, 20, 5);

    // Verify ASCII characters:
    EXPECT_TRUE(text.find("T1      |##..|") != std::string::npos); // Busy first 10ms, then idle
    EXPECT_TRUE(text.find("T2      |..##|") != std::string::npos); // Idle first 10ms, then busy
}

// TEST 6.2: Export execution records to a real CSV file on disk.
TEST(GanttTest, ExportTimelineCSV) {
    std::vector<JobRecord> records = {
        JobRecord{"T1", 1, 0, 0, 10, 20, 10, 10, true, 0, 0},
        JobRecord{"T2", 1, 0, 10, 20, 20, 10, 20, true, 10, 0}
    };
    const std::string test_csv = "test_timeline.csv";
    EXPECT_TRUE(export_timeline_csv(test_csv, records));

    // Read back the created CSV file to verify contents:
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
    std::remove(test_csv.c_str()); // Clean up temporary test file
}
