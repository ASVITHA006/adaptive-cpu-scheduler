#pragma once
#include "process.h"
#include "metrics.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────
//  Scheduling configuration knobs
// ─────────────────────────────────────────────
struct SchedulerConfig {
    std::string algorithm        = "adaptive";
    int         quantum          = 0;         // 0 = auto-compute
    int         starv_threshold  = 15;
    int         aging_boost      = 1;
    bool        verbose          = false;
    bool        context_switch_overhead = false; // future: add 1-unit CS overhead
};

// ─────────────────────────────────────────────
//  Adaptive decision record (explainability)
// ─────────────────────────────────────────────
struct AdaptiveDecision {
    std::string chosen_algorithm;
    std::string reasoning;         // human-readable explanation
    // Scoring table for all candidates
    struct CandidateScore {
        std::string algorithm;
        double      score;
        std::string reason;
    };
    std::vector<CandidateScore> scoring_table;
    WorkloadStats               workload_stats;
};

// ─────────────────────────────────────────────
//  Individual algorithm runners
//  Each returns a fully populated SimulationResult
// ─────────────────────────────────────────────
namespace Algorithms {

SimulationResult run_fcfs     (std::vector<Process> procs, const SchedulerConfig& cfg);
SimulationResult run_sjf      (std::vector<Process> procs, const SchedulerConfig& cfg);
SimulationResult run_srtf     (std::vector<Process> procs, const SchedulerConfig& cfg);
SimulationResult run_priority (std::vector<Process> procs, const SchedulerConfig& cfg);
SimulationResult run_priority_preemptive(std::vector<Process> procs, const SchedulerConfig& cfg);
SimulationResult run_rr       (std::vector<Process> procs, const SchedulerConfig& cfg, int quantum = 0);

} // namespace Algorithms

// ─────────────────────────────────────────────
//  Adaptive decision engine
// ─────────────────────────────────────────────
namespace AdaptiveEngine {

AdaptiveDecision decide(const std::vector<Process>& procs, const SchedulerConfig& cfg);
SimulationResult run_adaptive(std::vector<Process> procs, const SchedulerConfig& cfg);

} // namespace AdaptiveEngine

// ─────────────────────────────────────────────
//  Comparison mode: run all algorithms, rank
// ─────────────────────────────────────────────
namespace Comparison {

struct ComparisonReport {
    std::vector<SimulationResult>  results;
    std::string                    best_overall;  // by weighted score
    std::string                    best_fairness;
    std::string                    best_throughput;
    std::string                    lowest_wait;
    std::string                    fewest_switches;
    AdaptiveDecision               adaptive_decision;
};

ComparisonReport run_all(std::vector<Process> procs, const SchedulerConfig& cfg);

} // namespace Comparison

// ─────────────────────────────────────────────
//  Unified entry point
// ─────────────────────────────────────────────
SimulationResult run_scheduler(std::vector<Process> procs, const SchedulerConfig& cfg);
