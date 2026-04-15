#pragma once
#include "process.h"
#include <vector>

// ─────────────────────────────────────────────
//  Workload statistics (pre-simulation analysis)
// ─────────────────────────────────────────────
struct WorkloadStats {
    double avg_burst          = 0.0;
    double burst_variance     = 0.0;
    double burst_stddev       = 0.0;
    double median_burst       = 0.0;
    int    min_burst          = 0;
    int    max_burst          = 0;
    int    priority_spread    = 0;
    double system_load        = 0.0;   // processes per time unit
    int    process_count      = 0;
    double io_ratio           = 0.0;   // fraction of IO-bound processes
    double cpu_ratio          = 0.0;
    bool   has_priorities     = false;
};

// ─────────────────────────────────────────────
//  Metrics module (pure functions, no state)
// ─────────────────────────────────────────────
namespace Metrics {

// Compute aggregate stats over a finished simulation
void compute_process_metrics(SimulationResult& result);

// Jain's Fairness Index over waiting times: (Σwi)² / (n × Σwi²)
double jains_fairness_index(const std::vector<Process>& processes);

// Analyse raw process list before scheduling
WorkloadStats analyse_workload(const std::vector<Process>& processes);

// Compute dynamic RR quantum from workload stats
int compute_dynamic_quantum(const WorkloadStats& stats);

} // namespace Metrics
