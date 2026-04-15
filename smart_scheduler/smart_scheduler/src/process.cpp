#include "../include/process.h"
#include <numeric>
#include <cmath>
#include <algorithm>

double SimulationResult::avg_waiting_time() const {
    if (processes.empty()) return 0;
    double s = 0;
    for (auto& p : processes) s += p.waiting_time();
    return s / processes.size();
}

double SimulationResult::avg_turnaround_time() const {
    if (processes.empty()) return 0;
    double s = 0;
    for (auto& p : processes) s += p.turnaround_time();
    return s / processes.size();
}

double SimulationResult::avg_response_time() const {
    if (processes.empty()) return 0;
    double s = 0;
    for (auto& p : processes) s += p.response_time();
    return s / processes.size();
}

double SimulationResult::throughput() const {
    if (total_time == 0) return 0;
    return (double)processes.size() / total_time;
}

double SimulationResult::cpu_utilization() const {
    if (total_time == 0) return 0;
    return 100.0 * (total_time - idle_time) / total_time;
}

// Jain's Fairness Index = (sum wi)^2 / (n * sum wi^2)
// Quantitatively measures equitable CPU distribution among processes
double SimulationResult::jains_fairness_index() const {
    if (processes.empty()) return 1.0;
    double sum = 0, sum_sq = 0;
    for (auto& p : processes) {
        double w = std::max(0, p.waiting_time());
        sum    += w;
        sum_sq += w * w;
    }
    // If all waiting times are 0, scheduling is perfectly fair
    if (sum_sq == 0) return 1.0;
    int n = processes.size();
    return (sum * sum) / ((double)n * sum_sq);
}
