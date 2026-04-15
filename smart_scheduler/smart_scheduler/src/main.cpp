#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/io_utils.h"
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

void print_usage(const char* prog) {
    std::cerr << "Smart Scheduler — Adaptive CPU Scheduling Framework\n"
              << "Usage: " << prog << " [OPTIONS]\n"
              << "\nOptions:\n"
              << "  --config <file>       Path to JSON config file (default: config/processes.json)\n"
              << "  --algo <name>         Algorithm: FCFS|SJF|SRTF|Priority|PriorityP|RR|adaptive\n"
              << "  --quantum <n>         Round Robin time quantum (default: auto-computed)\n"
              << "  --starv-threshold <n> Starvation/waiting time threshold in time units (default: 15)\n"
              << "  --aging-boost <n>     Priority boost per threshold cycle (default: 1)\n"
              << "  --output <file>       Write JSON output to file (default: stdout)\n"
              << "  --help                Show this help\n"
              << "\nExample:\n"
              << "  " << prog << " --config config/processes.json --algo adaptive\n"
              << "  " << prog << " --algo RR --quantum 4\n"
              << "  " << prog << " --algo Priority --starv-threshold 10 --aging-boost 2\n";
}

int main(int argc, char* argv[]) {
    std::string config_path = "config/processes.json";
    std::string algo_str    = "adaptive";
    std::string output_path = "";
    int quantum             = -1;   // -1 = auto (dynamic quantum)
    int starv_threshold     = 15;
    int aging_boost         = 1;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if      (arg == "--help" || arg == "-h")          { print_usage(argv[0]); return 0; }
        else if (arg == "--config"          && i+1 < argc) config_path     = argv[++i];
        else if (arg == "--algo"            && i+1 < argc) algo_str        = argv[++i];
        else if (arg == "--quantum"         && i+1 < argc) quantum         = std::stoi(argv[++i]);
        else if (arg == "--starv-threshold" && i+1 < argc) starv_threshold = std::stoi(argv[++i]);
        else if (arg == "--aging-boost"     && i+1 < argc) aging_boost     = std::stoi(argv[++i]);
        else if (arg == "--output"          && i+1 < argc) output_path     = argv[++i];
        else { std::cerr << "Unknown argument: " << arg << "\n"; print_usage(argv[0]); return 1; }
    }

    // Load processes
    std::vector<Process> procs;
    try {
        procs = load_config(config_path);
    } catch (std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << "\n";
        return 1;
    }
    if (procs.empty()) { std::cerr << "No processes loaded.\n"; return 1; }

    // Run adaptive selection (for metadata and dynamic quantum — pass starv_threshold)
    AdaptiveDecision dec = adaptive_select(procs, starv_threshold);
    if (quantum > 0) dec.quantum = quantum;   // manual override

    // Run simulation
    SimulationResult result;
    Algorithm algo = parse_algorithm(algo_str);
    try {
        switch (algo) {
            case Algorithm::FCFS:
                result = run_fcfs(procs); break;
            case Algorithm::SJF:
                result = run_sjf(procs); break;
            case Algorithm::SRTF:
                result = run_srtf(procs); break;
            case Algorithm::PRIORITY:
                result = run_priority(procs, false, starv_threshold, aging_boost); break;
            case Algorithm::PRIORITY_PREEMPTIVE:
                result = run_priority(procs, true, starv_threshold, aging_boost); break;
            case Algorithm::ROUND_ROBIN:
                result = run_round_robin(procs, dec.quantum); break;
            case Algorithm::ADAPTIVE:
                result = run_adaptive(procs, starv_threshold, aging_boost); break;
        }
    } catch (std::exception& e) {
        std::cerr << "Simulation error: " << e.what() << "\n";
        return 1;
    }

    // Output JSON
    std::string json = result_to_json(result, dec);
    if (output_path.empty()) {
        std::cout << json;
    } else {
        std::ofstream f(output_path);
        if (!f.is_open()) { std::cerr << "Cannot write to: " << output_path << "\n"; return 1; }
        f << json;
        std::cerr << "Result written to " << output_path << "\n";
    }
    return 0;
}
