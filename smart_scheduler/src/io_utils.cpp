#include "io_utils.h"
#include "metrics.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <algorithm>

// ─────────────────────────────────────────────
//  Tiny JSON helpers (no external lib needed)
// ─────────────────────────────────────────────
static std::string jstr(const std::string& s) { return "\"" + s + "\""; }
static std::string jkv(const std::string& k, const std::string& v, bool last = false) {
    return "\"" + k + "\": " + v + (last ? "" : ",") + "\n";
}
static std::string jkv(const std::string& k, double v, bool last = false) {
    std::ostringstream oss; oss << std::fixed << std::setprecision(4) << v;
    return jkv(k, oss.str(), last);
}
static std::string jkv(const std::string& k, int v, bool last = false) {
    return jkv(k, std::to_string(v), last);
}

namespace IO {

// ─────────────────────────────────────────────
//  Config loader (minimal hand-rolled JSON parser)
// ─────────────────────────────────────────────
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\"");
    size_t b = s.find_last_not_of(" \t\r\n\",");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}
static std::string extract(const std::string& line, const std::string& key) {
    size_t pos = line.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = line.find(":", pos);
    if (pos == std::string::npos) return "";
    return trim(line.substr(pos + 1));
}

LoadedConfig load_config(const std::string& path) {
    LoadedConfig cfg;
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open config: " + path);

    std::string content, line;
    while (std::getline(f, line)) content += line + "\n";

    // Helper lambda: find integer value for a key in a substring
    auto find_int = [](const std::string& s, const std::string& key, int def) -> int {
        size_t pos = s.find("\"" + key + "\"");
        if (pos == std::string::npos) return def;
        size_t colon = s.find(":", pos + key.size() + 2);
        if (colon == std::string::npos) return def;
        size_t start = s.find_first_not_of(" \t\r\n", colon + 1);
        if (start == std::string::npos) return def;
        try { return std::stoi(s.substr(start)); } catch (...) { return def; }
    };

    // Helper lambda: find string value for a key in a substring
    auto find_str = [](const std::string& s, const std::string& key, std::string def) -> std::string {
        size_t pos = s.find("\"" + key + "\"");
        if (pos == std::string::npos) return def;
        size_t colon = s.find(":", pos + key.size() + 2);
        if (colon == std::string::npos) return def;
        size_t q1 = s.find("\"", colon + 1);
        if (q1 == std::string::npos) return def;
        size_t q2 = s.find("\"", q1 + 1);
        if (q2 == std::string::npos) return def;
        return s.substr(q1 + 1, q2 - q1 - 1);
    };

    cfg.starvation_threshold = find_int(content, "starvation_threshold", 15);
    cfg.aging_boost          = find_int(content, "aging_boost", 1);

    // Parse process objects between { }
    size_t proc_start = content.find("\"processes\"");
    if (proc_start == std::string::npos) return cfg;

    size_t arr_start = content.find("[", proc_start);
    size_t arr_end   = content.rfind("]");
    if (arr_start == std::string::npos || arr_end == std::string::npos) return cfg;

    size_t pos = arr_start;
    while (pos < arr_end) {
        size_t obj_start = content.find("{", pos);
        if (obj_start == std::string::npos || obj_start >= arr_end) break;
        size_t obj_end = content.find("}", obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj = content.substr(obj_start, obj_end - obj_start + 1);

        Process p;
        p.id           = find_str(obj, "id", "P?");
        p.arrival_time = find_int(obj, "arrival", 0);
        p.burst_time   = find_int(obj, "burst", 1);
        p.priority     = find_int(obj, "priority", 1);

        std::string tp = find_str(obj, "type", "CPU");
        if (tp == "IO")       p.type = ProcessType::IO_BOUND;
        else if (tp == "MIXED") p.type = ProcessType::MIXED;
        else                    p.type = ProcessType::CPU_BOUND;

        p.remaining_time    = p.burst_time;
        p.effective_priority = p.priority;
        p.original_priority  = p.priority;
        cfg.processes.push_back(p);
        pos = obj_end + 1;
    }

    return cfg;
}

// ─────────────────────────────────────────────
//  Workload Generator
// ─────────────────────────────────────────────
std::vector<Process> generate_workload(const std::string& preset, int seed) {
    std::mt19937 rng(seed);
    std::vector<Process> procs;

    auto make = [&](const std::string& id, int arr, int burst, int pri, ProcessType t) {
        Process p;
        p.id = id; p.arrival_time = arr; p.burst_time = burst;
        p.priority = pri; p.type = t;
        p.remaining_time = burst; p.effective_priority = pri; p.original_priority = pri;
        return p;
    };

    if (preset == "random") {
        std::uniform_int_distribution<int> arr(0, 10), burst(1, 20), pri(1, 5);
        for (int i = 0; i < 8; i++) {
            ProcessType t = (i % 3 == 0) ? ProcessType::IO_BOUND : ProcessType::CPU_BOUND;
            procs.push_back(make("P" + std::to_string(i+1), arr(rng), burst(rng), pri(rng), t));
        }
    } else if (preset == "heavy") {
        std::uniform_int_distribution<int> burst(10, 30), arr(0, 3);
        for (int i = 0; i < 12; i++)
            procs.push_back(make("P" + std::to_string(i+1), arr(rng), burst(rng), i % 4 + 1, ProcessType::CPU_BOUND));
    } else if (preset == "light") {
        std::uniform_int_distribution<int> burst(1, 5);
        for (int i = 0; i < 5; i++)
            procs.push_back(make("P" + std::to_string(i+1), i * 2, burst(rng), 1, ProcessType::IO_BOUND));
    } else if (preset == "bursty") {
        // Mix of very short and very long bursts
        procs.push_back(make("P1",  0,  2, 3, ProcessType::IO_BOUND));
        procs.push_back(make("P2",  0, 30, 1, ProcessType::CPU_BOUND));
        procs.push_back(make("P3",  1,  1, 4, ProcessType::IO_BOUND));
        procs.push_back(make("P4",  2, 25, 2, ProcessType::CPU_BOUND));
        procs.push_back(make("P5",  3,  3, 3, ProcessType::MIXED));
        procs.push_back(make("P6",  4, 18, 2, ProcessType::CPU_BOUND));
    } else if (preset == "uniform") {
        for (int i = 0; i < 6; i++)
            procs.push_back(make("P" + std::to_string(i+1), i, 5, 2, ProcessType::CPU_BOUND));
    } else if (preset == "edge") {
        // Single process, identical arrivals, zero burst
        procs.push_back(make("P1", 0, 1, 1, ProcessType::CPU_BOUND));
        procs.push_back(make("P2", 0, 1, 1, ProcessType::CPU_BOUND));
        procs.push_back(make("P3", 100, 5, 5, ProcessType::IO_BOUND));
    } else {
        // default: balanced
        procs = {
            make("P1", 0, 6, 2, ProcessType::CPU_BOUND),
            make("P2", 1, 4, 1, ProcessType::IO_BOUND),
            make("P3", 2, 8, 3, ProcessType::CPU_BOUND),
            make("P4", 3, 2, 1, ProcessType::IO_BOUND),
            make("P5", 4, 5, 4, ProcessType::MIXED),
        };
    }

    return procs;
}

// ─────────────────────────────────────────────
//  JSON serialisation — single result
// ─────────────────────────────────────────────
std::string result_to_json(const SimulationResult& r, const AdaptiveDecision* d) {
    std::ostringstream o;
    o << "{\n";
    o << jkv("algorithm", jstr(r.algorithm_name));
    o << jkv("avg_waiting_time",    r.avg_waiting_time);
    o << jkv("avg_turnaround_time", r.avg_turnaround_time);
    o << jkv("avg_response_time",   r.avg_response_time);
    o << jkv("throughput",          r.throughput);
    o << jkv("cpu_utilization",     r.cpu_utilization);
    o << jkv("fairness_index",      r.fairness_index);
    o << jkv("context_switches",    r.context_switches);
    o << jkv("context_switch_rate", r.context_switch_rate);
    o << jkv("total_idle_time",     r.total_idle_time);
    o << jkv("makespan",            r.makespan);
    o << jkv("initial_quantum",     r.initial_quantum);
    o << jkv("final_quantum",       r.final_quantum);

    // Processes
    o << "\"processes\": [\n";
    for (size_t i = 0; i < r.processes.size(); i++) {
        const auto& p = r.processes[i];
        o << "  {\n";
        o << "    " << jkv("id",               jstr(p.id));
        o << "    " << jkv("arrival_time",      p.arrival_time);
        o << "    " << jkv("burst_time",        p.burst_time);
        o << "    " << jkv("priority",          p.priority);
        o << "    " << jkv("effective_priority",p.effective_priority);
        o << "    " << jkv("waiting_time",      p.waiting_time);
        o << "    " << jkv("turnaround_time",   p.turnaround_time);
        o << "    " << jkv("response_time",     p.response_time);
        o << "    " << jkv("start_time",        p.start_time);
        o << "    " << jkv("finish_time",       p.finish_time);
        o << "    " << jkv("priority_boosts",   p.priority_boosts, true);
        o << "  }" << (i + 1 < r.processes.size() ? "," : "") << "\n";
    }
    o << "],\n";

    // Timeline
    o << "\"timeline\": [\n";
    for (size_t i = 0; i < r.timeline.size(); i++) {
        const auto& seg = r.timeline[i];
        o << "  {\"pid\": " << jstr(seg.process_id)
          << ", \"start\": " << seg.start
          << ", \"end\": "   << seg.end
          << ", \"idle\": "  << (seg.is_idle ? "true" : "false")
          << "}" << (i + 1 < r.timeline.size() ? "," : "") << "\n";
    }
    o << "],\n";

    // Aging events
    o << "\"aging_events\": [\n";
    for (size_t i = 0; i < r.aging_events.size(); i++) {
        const auto& e = r.aging_events[i];
        o << "  {\"time\": " << e.time
          << ", \"pid\": "   << jstr(e.process_id)
          << ", \"old_priority\": " << e.old_priority
          << ", \"new_priority\": " << e.new_priority
          << ", \"wait\": "  << e.accumulated_wait
          << "}" << (i + 1 < r.aging_events.size() ? "," : "") << "\n";
    }
    o << "],\n";

    // Starvation warnings
    o << "\"starvation_warnings\": [\n";
    for (size_t i = 0; i < r.starvation_warnings.size(); i++) {
        const auto& w = r.starvation_warnings[i];
        o << "  {\"pid\": "         << jstr(w.process_id)
          << ", \"detected_at\": "  << w.detected_at
          << ", \"wait_duration\": " << w.wait_duration
          << "}" << (i + 1 < r.starvation_warnings.size() ? "," : "") << "\n";
    }
    o << "]";

    // Adaptive decision
    if (d) {
        o << ",\n\"adaptive_decision\": {\n";
        o << jkv("chosen", jstr(d->chosen_algorithm));
        o << jkv("reasoning", jstr(d->reasoning));
        o << "\"scoring_table\": [\n";
        for (size_t i = 0; i < d->scoring_table.size(); i++) {
            const auto& sc = d->scoring_table[i];
            o << "  {\"algorithm\": " << jstr(sc.algorithm)
              << ", \"score\": " << std::fixed << std::setprecision(1) << sc.score
              << ", \"reason\": " << jstr(sc.reason)
              << "}" << (i + 1 < d->scoring_table.size() ? "," : "") << "\n";
        }
        o << "],\n";
        o << "\"workload\": {\n";
        const auto& ws = d->workload_stats;
        o << jkv("avg_burst",      ws.avg_burst);
        o << jkv("burst_variance", ws.burst_variance);
        o << jkv("system_load",    ws.system_load);
        o << jkv("priority_spread", ws.priority_spread);
        o << jkv("process_count",  ws.process_count);
        o << jkv("io_ratio",       ws.io_ratio, true);
        o << "}\n}";
    }

    o << "\n}";
    return o.str();
}

// ─────────────────────────────────────────────
//  JSON — comparison report
// ─────────────────────────────────────────────
std::string comparison_to_json(const Comparison::ComparisonReport& report) {
    std::ostringstream o;
    o << "{\n";
    o << jkv("best_overall",    jstr(report.best_overall));
    o << jkv("best_fairness",   jstr(report.best_fairness));
    o << jkv("best_throughput", jstr(report.best_throughput));
    o << jkv("lowest_wait",     jstr(report.lowest_wait));
    o << jkv("fewest_switches", jstr(report.fewest_switches));

    // Include adaptive decision
    o << "\"adaptive_decision\": {\n";
    o << jkv("chosen", jstr(report.adaptive_decision.chosen_algorithm));
    o << jkv("reasoning", jstr(report.adaptive_decision.reasoning), true);
    o << "},\n";

    // Results array
    o << "\"results\": [\n";
    for (size_t i = 0; i < report.results.size(); i++) {
        const auto& r = report.results[i];
        o << "{\n";
        o << jkv("algorithm",           jstr(r.algorithm_name));
        o << jkv("avg_waiting_time",    r.avg_waiting_time);
        o << jkv("avg_turnaround_time", r.avg_turnaround_time);
        o << jkv("avg_response_time",   r.avg_response_time);
        o << jkv("throughput",          r.throughput);
        o << jkv("cpu_utilization",     r.cpu_utilization);
        o << jkv("fairness_index",      r.fairness_index);
        o << jkv("context_switches",    r.context_switches);
        o << jkv("makespan",            r.makespan);

        // Include timeline + processes for each result
        o << "\"processes\": [\n";
        for (size_t j = 0; j < r.processes.size(); j++) {
            const auto& p = r.processes[j];
            o << "  {\"id\": " << jstr(p.id)
              << ", \"waiting_time\": " << p.waiting_time
              << ", \"turnaround_time\": " << p.turnaround_time
              << ", \"response_time\": " << p.response_time
              << "}" << (j + 1 < r.processes.size() ? "," : "") << "\n";
        }
        o << "],\n";

        o << "\"timeline\": [\n";
        for (size_t j = 0; j < r.timeline.size(); j++) {
            const auto& seg = r.timeline[j];
            o << "  {\"pid\": " << jstr(seg.process_id)
              << ", \"start\": " << seg.start
              << ", \"end\": " << seg.end
              << ", \"idle\": " << (seg.is_idle ? "true" : "false")
              << "}" << (j + 1 < r.timeline.size() ? "," : "") << "\n";
        }
        o << "]\n";

        o << "}" << (i + 1 < report.results.size() ? "," : "") << "\n";
    }
    o << "]\n}";
    return o.str();
}

// ─────────────────────────────────────────────
//  Pretty CLI table output
// ─────────────────────────────────────────────
static std::string bar(int w, char c = '─') { return std::string(w, c); }
static std::string col(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}

void print_result_table(const SimulationResult& r) {
    std::cout << "\n┌" << bar(70) << "┐\n";
    std::cout << "│  Algorithm: " << col(r.algorithm_name, 55) << "  │\n";
    std::cout << "├" << bar(70) << "┤\n";
    std::cout << "│ " << col("Process", 8) << col("Arrival", 8)
              << col("Burst", 7) << col("Wait", 7) << col("TAT", 7)
              << col("RT", 7) << col("Finish", 7) << col("Priority Boosts", 15) << " │\n";
    std::cout << "├" << bar(70) << "┤\n";
    for (const auto& p : r.processes) {
        std::cout << "│ "
                  << col(p.id, 8)
                  << col(std::to_string(p.arrival_time), 8)
                  << col(std::to_string(p.burst_time), 7)
                  << col(std::to_string(p.waiting_time), 7)
                  << col(std::to_string(p.turnaround_time), 7)
                  << col(std::to_string(p.response_time), 7)
                  << col(std::to_string(p.finish_time), 7)
                  << col(std::to_string(p.priority_boosts), 15) << " │\n";
    }
    std::cout << "├" << bar(70) << "┤\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "│  Avg Wait: "   << col(std::to_string(r.avg_waiting_time),    10)
              << "  Avg TAT: "     << col(std::to_string(r.avg_turnaround_time), 10)
              << "  Avg RT: "      << col(std::to_string(r.avg_response_time),   12) << " │\n";
    std::cout << "│  CPU Util: "   << col(std::to_string(r.cpu_utilization) + "%", 10)
              << "  Throughput: "  << col(std::to_string(r.throughput),          10)
              << "  Fairness: "    << col(std::to_string(r.fairness_index),      10) << " │\n";
    std::cout << "│  Ctx Switches: " << col(std::to_string(r.context_switches), 6)
              << "  Idle Time: "   << col(std::to_string(r.total_idle_time),     6)
              << "  Quantum: "     << col(std::to_string(r.final_quantum),       6) << "           │\n";
    std::cout << "└" << bar(70) << "┘\n";
}

void print_comparison_table(const Comparison::ComparisonReport& report) {
    std::cout << "\n╔" << bar(80, '═') << "╗\n";
    std::cout << "║  COMPARISON REPORT" << std::string(60, ' ') << "║\n";
    std::cout << "╠" << bar(80, '═') << "╣\n";
    std::cout << "║ " << col("Algorithm", 12) << col("AvgWait", 10) << col("AvgTAT", 10)
              << col("AvgRT", 8) << col("CtxSw", 7) << col("Fairness", 10)
              << col("CpuUtil%", 10) << col("Thruput", 10) << " ║\n";
    std::cout << "╠" << bar(80, '═') << "╣\n";
    for (const auto& r : report.results) {
        bool is_best = (r.algorithm_name == report.best_overall);
        std::cout << "║" << (is_best ? "*" : " ")
                  << col(r.algorithm_name, 12)
                  << col(std::to_string(r.avg_waiting_time).substr(0,6), 10)
                  << col(std::to_string(r.avg_turnaround_time).substr(0,6), 10)
                  << col(std::to_string(r.avg_response_time).substr(0,6), 8)
                  << col(std::to_string(r.context_switches), 7)
                  << col(std::to_string(r.fairness_index).substr(0,6), 10)
                  << col(std::to_string(r.cpu_utilization).substr(0,5) + "%", 10)
                  << col(std::to_string(r.throughput).substr(0,6), 10) << " ║\n";
    }
    std::cout << "╠" << bar(80, '═') << "╣\n";
    std::cout << "║  * = Best Overall (weighted score)"
              << std::string(42, ' ') << "║\n";
    std::cout << "║  Best Fairness:    " << col(report.best_fairness, 15)
              << "  Best Throughput: " << col(report.best_throughput, 15) << "      ║\n";
    std::cout << "║  Fewest Switches:  " << col(report.fewest_switches, 15)
              << "  Lowest Wait:     " << col(report.lowest_wait, 15) << "      ║\n";
    std::cout << "╠" << bar(80, '═') << "╣\n";
    std::cout << "║  Adaptive Engine → " << report.adaptive_decision.chosen_algorithm
              << std::string(60 - report.adaptive_decision.chosen_algorithm.size(), ' ') << "║\n";
    std::cout << "╚" << bar(80, '═') << "╝\n";
}

} // namespace IO
