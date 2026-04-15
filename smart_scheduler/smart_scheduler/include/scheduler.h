#pragma once
#include "process.h"

enum class Algorithm { FCFS, SJF, SRTF, PRIORITY, PRIORITY_PREEMPTIVE, ROUND_ROBIN, ADAPTIVE };

// Adaptive decision engine — all inputs described in the abstract
struct AdaptiveDecision {
    Algorithm algo;
    std::string reason;
    int    quantum;
    double avg_burst;
    double burst_variance;
    int    priority_spread;
    // New fields to satisfy abstract requirements
    double system_load;            // processes / time unit
    double context_switch_rate;    // estimated CS frequency
    double waiting_time_threshold; // effective starvation threshold used
};

AdaptiveDecision adaptive_select(const std::vector<Process>& procs,
                                  int starv_threshold = 15);

// Dynamic quantum: computes optimal quantum from workload statistics
int compute_dynamic_quantum(const std::vector<Process>& procs);

// Individual schedulers
SimulationResult run_fcfs(std::vector<Process> procs);
SimulationResult run_sjf(std::vector<Process> procs);
SimulationResult run_srtf(std::vector<Process> procs);
SimulationResult run_priority(std::vector<Process> procs, bool preemptive,
                              int starv_threshold, int aging_boost);
SimulationResult run_round_robin(std::vector<Process> procs, int quantum);
SimulationResult run_adaptive(std::vector<Process> procs,
                              int starv_threshold, int aging_boost);

std::string algorithm_name(Algorithm a);
