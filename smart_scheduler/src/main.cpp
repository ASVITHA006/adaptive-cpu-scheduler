#include "scheduler.h"
#include "io_utils.h"
#include "metrics.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

// ─────────────────────────────────────────────
//  CLI usage
// ─────────────────────────────────────────────
static void usage() {
    std::cerr <<
R"(
Smart Scheduler — Adaptive CPU Scheduling Framework
====================================================
Usage: scheduler [options]

Options:
  --config <file>          JSON config file               (default: config/processes.json)
  --algo <name>            Algorithm: FCFS | SJF | SRTF |
                             Priority | PriorityP | RR | adaptive | compare
                                                          (default: adaptive)
  --quantum <n>            RR quantum (0 = auto-compute)  (default: 0)
  --starv-threshold <n>    Starvation detection threshold  (default: 15)
  --aging-boost <n>        Priority boost per cycle        (default: 1)
  --output <file>          Write JSON to file              (default: stdout)
  --generate <preset>      Generate workload: random | heavy | light |
                             bursty | uniform | edge       (overrides --config)
  --seed <n>               RNG seed for --generate         (default: 42)
  --verbose                Print formatted tables to stderr
  --help                   Show this help

Examples:
  scheduler --algo adaptive
  scheduler --algo compare
  scheduler --algo RR --quantum 4
  scheduler --generate bursty --algo compare
  scheduler --generate heavy --algo SRTF --verbose
)";
}

int main(int argc, char* argv[]) {
    // ── Parse args ──────────────────────────────
    SchedulerConfig cfg;
    std::string config_path = "config/processes.json";
    std::string output_path = "";
    std::string generate_preset = "";
    int seed = 42;
    bool verbose = false;
    bool show_help = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 < argc) return argv[++i];
            std::cerr << "Missing value for " << arg << "\n"; exit(1);
        };

        if (arg == "--config")           config_path        = next();
        else if (arg == "--algo")        cfg.algorithm      = next();
        else if (arg == "--quantum")     cfg.quantum        = std::stoi(next());
        else if (arg == "--starv-threshold") cfg.starv_threshold = std::stoi(next());
        else if (arg == "--aging-boost") cfg.aging_boost    = std::stoi(next());
        else if (arg == "--output")      output_path        = next();
        else if (arg == "--generate")    generate_preset    = next();
        else if (arg == "--seed")        seed               = std::stoi(next());
        else if (arg == "--verbose")     verbose            = true;
        else if (arg == "--help")        show_help          = true;
        else { std::cerr << "Unknown option: " << arg << "\n"; usage(); return 1; }
    }

    if (show_help) { usage(); return 0; }

    // ── Load / generate processes ────────────────
    std::vector<Process> procs;
    try {
        if (!generate_preset.empty()) {
            procs = IO::generate_workload(generate_preset, seed);
        } else {
            auto loaded = IO::load_config(config_path);
            procs = loaded.processes;
            if (cfg.starv_threshold == 15) cfg.starv_threshold = loaded.starvation_threshold;
            if (cfg.aging_boost == 1)      cfg.aging_boost     = loaded.aging_boost;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << "\n";
        // Fallback to a simple default workload
        procs = IO::generate_workload("random", seed);
    }

    if (procs.empty()) {
        std::cerr << "No processes to schedule.\n";
        return 1;
    }

    // ── Run scheduler ────────────────────────────
    std::string json_output;

    if (cfg.algorithm == "compare") {
        auto report = Comparison::run_all(procs, cfg);
        json_output = IO::comparison_to_json(report);
        if (verbose) IO::print_comparison_table(report);
    } else {
        AdaptiveDecision* decision_ptr = nullptr;
        AdaptiveDecision  decision;
        bool is_adaptive = (cfg.algorithm == "adaptive");

        SimulationResult result;
        if (is_adaptive) {
            decision     = AdaptiveEngine::decide(procs, cfg);
            decision_ptr = &decision;
            result       = AdaptiveEngine::run_adaptive(procs, cfg);
        } else {
            result = run_scheduler(procs, cfg);
        }

        json_output = IO::result_to_json(result, decision_ptr);
        if (verbose) IO::print_result_table(result);
    }

    // ── Output ───────────────────────────────────
    if (output_path.empty()) {
        std::cout << json_output << std::endl;
    } else {
        std::ofstream f(output_path);
        if (!f.is_open()) { std::cerr << "Cannot write to " << output_path << "\n"; return 1; }
        f << json_output;
        std::cerr << "Output written to " << output_path << "\n";
    }

    return 0;
}
