#pragma once
#include <string>
#include <vector>

enum class ProcessType { CPU, IO, MIXED };

struct Process {
    std::string id;
    int arrival_time;
    int burst_time;
    int priority;          // lower = higher priority
    ProcessType type;

    // Runtime fields
    int remaining_time;
    int effective_priority;
    int completion_time  = 0;
    int first_run_time   = -1;
    int wait_since       = 0;
    bool completed       = false;

    int turnaround_time() const { return completion_time - arrival_time; }
    int waiting_time()    const { return turnaround_time() - burst_time; }
    int response_time()   const { return first_run_time - arrival_time; }
};

struct TimelineSegment {
    std::string pid;
    int start;
    int end;
};

struct AgingEvent {
    std::string pid;
    int time;
    int old_priority;
    int new_priority;
    int wait_time;
};

struct SimulationResult {
    std::string algorithm;
    int quantum;
    std::vector<TimelineSegment> timeline;
    std::vector<Process> processes;
    std::vector<AgingEvent> aging_events;
    int context_switches;
    int idle_time;
    int total_time;
    // New fields from abstract requirements
    double system_load          = 0.0;  // processes / total_time
    double context_switch_rate  = 0.0;  // context_switches / total_time

    double avg_waiting_time()    const;
    double avg_turnaround_time() const;
    double avg_response_time()   const;
    double throughput()          const;
    double cpu_utilization()     const;
    double jains_fairness_index() const;
};
