#pragma once
#include "process.h"
#include "scheduler.h"
#include <string>
#include <vector>

namespace IO {

// ─────────────────────────────────────────────
//  Input loading
// ─────────────────────────────────────────────
struct LoadedConfig {
    std::vector<Process> processes;
    int starvation_threshold = 15;
    int aging_boost          = 1;
};

LoadedConfig load_config(const std::string& path);
std::vector<Process> generate_workload(const std::string& preset, int seed = 42);
// presets: "random", "heavy", "light", "bursty", "uniform", "edge"

// ─────────────────────────────────────────────
//  Output serialisation
// ─────────────────────────────────────────────
std::string result_to_json(const SimulationResult& result,
                           const AdaptiveDecision* decision = nullptr);

std::string comparison_to_json(const Comparison::ComparisonReport& report);

// Pretty CLI table output
void print_result_table(const SimulationResult& result);
void print_comparison_table(const Comparison::ComparisonReport& report);

} // namespace IO
