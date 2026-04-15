#pragma once
#include <string>
#include <vector>

// ─────────────────────────────────────────────
//  Process types for workload classification
// ─────────────────────────────────────────────
enum class ProcessType { CPU_BOUND, IO_BOUND, MIXED };

// ─────────────────────────────────────────────
//  Core process descriptor
// ─────────────────────────────────────────────
struct Process {
    std::string id;
    int         arrival_time   = 0;
    int         burst_time     = 0;
    int         priority       = 0;          // lower number = higher priority
    ProcessType type           = ProcessType::CPU_BOUND;

    // Runtime state (mutated during simulation)
    int remaining_time         = 0;
    int effective_priority     = 0;          // modified by aging
    int start_time             = -1;
    int finish_time            = -1;
    int last_scheduled_time    = -1;

    // Derived metrics (filled post-simulation)
    int waiting_time           = 0;
    int turnaround_time        = 0;
    int response_time          = 0;

    // Aging metadata
    int priority_boosts        = 0;
    int original_priority      = 0;

    bool is_completed() const { return remaining_time <= 0; }
    void reset(int bt) {
        remaining_time   = bt;
        effective_priority = priority;
        start_time       = -1;
        finish_time      = -1;
        last_scheduled_time = -1;
        waiting_time     = 0;
        turnaround_time  = 0;
        response_time    = 0;
        priority_boosts  = 0;
        original_priority = priority;
    }
};

// ─────────────────────────────────────────────
//  One slice on the Gantt chart
// ─────────────────────────────────────────────
struct TimelineSegment {
    std::string process_id;   // "IDLE" for idle periods
    int         start        = 0;
    int         end          = 0;
    bool        is_idle      = false;
    bool        is_context_switch = false;
};

// ─────────────────────────────────────────────
//  Aging event log entry
// ─────────────────────────────────────────────
struct AgingEvent {
    int         time;
    std::string process_id;
    int         old_priority;
    int         new_priority;
    int         accumulated_wait;
};

// ─────────────────────────────────────────────
//  Starvation warning
// ─────────────────────────────────────────────
struct StarvationWarning {
    std::string process_id;
    int         detected_at;
    int         wait_duration;
};

// ─────────────────────────────────────────────
//  Full simulation result bundle
// ─────────────────────────────────────────────
struct SimulationResult {
    std::string               algorithm_name;
    std::vector<TimelineSegment> timeline;
    std::vector<Process>      processes;
    std::vector<AgingEvent>   aging_events;
    std::vector<StarvationWarning> starvation_warnings;

    // Aggregate metrics
    double avg_waiting_time    = 0.0;
    double avg_turnaround_time = 0.0;
    double avg_response_time   = 0.0;
    double throughput          = 0.0;
    double cpu_utilization     = 0.0;
    double fairness_index      = 0.0;   // Jain's Fairness Index
    int    context_switches    = 0;
    double context_switch_rate = 0.0;
    int    total_idle_time     = 0;
    int    makespan            = 0;

    // Dynamic quantum info (for RR)
    int    initial_quantum     = 0;
    int    final_quantum       = 0;
};
