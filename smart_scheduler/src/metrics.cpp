#include "metrics.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <numeric>

namespace Metrics {

// ─────────────────────────────────────────────
//  Post-simulation: fill per-process metrics and
//  compute aggregates on the SimulationResult.
// ─────────────────────────────────────────────
void compute_process_metrics(SimulationResult& result) {
    int n = static_cast<int>(result.processes.size());
    if (n == 0) return;

    // Walk timeline to count context switches and idle time
    int idle_time      = 0;
    int ctx_switches   = 0;
    std::string prev   = "";

    for (const auto& seg : result.timeline) {
        if (seg.is_idle) {
            idle_time += seg.end - seg.start;
        } else if (!prev.empty() && prev != seg.process_id && !seg.is_idle) {
            ctx_switches++;
        }
        prev = seg.process_id;
    }

    result.total_idle_time   = idle_time;
    result.context_switches  = ctx_switches;

    // Per-process derived metrics
    double sum_wt = 0, sum_tat = 0, sum_rt = 0;
    for (auto& p : result.processes) {
        p.turnaround_time = p.finish_time - p.arrival_time;
        p.waiting_time    = p.turnaround_time - p.burst_time;
        if (p.waiting_time < 0) p.waiting_time = 0;
        p.response_time   = (p.start_time >= 0) ? (p.start_time - p.arrival_time) : 0;
        if (p.response_time < 0) p.response_time = 0;
        sum_wt  += p.waiting_time;
        sum_tat += p.turnaround_time;
        sum_rt  += p.response_time;
    }

    result.avg_waiting_time    = sum_wt  / n;
    result.avg_turnaround_time = sum_tat / n;
    result.avg_response_time   = sum_rt  / n;

    // Makespan = finish time of last completed process
    int makespan = 0;
    for (const auto& p : result.processes)
        makespan = std::max(makespan, p.finish_time);
    result.makespan = makespan;

    result.throughput          = (makespan > 0) ? static_cast<double>(n) / makespan : 0.0;
    result.cpu_utilization     = (makespan > 0)
                                   ? 100.0 * (makespan - idle_time) / makespan
                                   : 0.0;
    result.context_switch_rate = (makespan > 0)
                                   ? static_cast<double>(ctx_switches) / makespan
                                   : 0.0;
    result.fairness_index      = jains_fairness_index(result.processes);
}

// ─────────────────────────────────────────────
//  Jain's Fairness Index
//  JFI = (Σwi)² / (n × Σwi²)
//  1.0 = perfectly fair, lower = more unfair
// ─────────────────────────────────────────────
double jains_fairness_index(const std::vector<Process>& processes) {
    int n = static_cast<int>(processes.size());
    if (n == 0) return 1.0;

    double sum   = 0.0;
    double sum_sq = 0.0;
    for (const auto& p : processes) {
        // Use (waiting_time + 1) to avoid division issues when wt = 0
        double w = static_cast<double>(p.waiting_time + 1);
        sum    += w;
        sum_sq += w * w;
    }
    if (sum_sq == 0.0) return 1.0;
    return (sum * sum) / (n * sum_sq);
}

// ─────────────────────────────────────────────
//  Pre-simulation workload analysis
// ─────────────────────────────────────────────
WorkloadStats analyse_workload(const std::vector<Process>& processes) {
    WorkloadStats s;
    s.process_count = static_cast<int>(processes.size());
    if (s.process_count == 0) return s;

    // Burst stats
    std::vector<int> bursts;
    bursts.reserve(s.process_count);
    double sum = 0.0;
    int min_pri = INT_MAX, max_pri = INT_MIN;
    int io_count = 0, cpu_count = 0;

    for (const auto& p : processes) {
        bursts.push_back(p.burst_time);
        sum += p.burst_time;
        if (p.priority < min_pri) min_pri = p.priority;
        if (p.priority > max_pri) max_pri = p.priority;
        if (p.type == ProcessType::IO_BOUND)       io_count++;
        else if (p.type == ProcessType::CPU_BOUND) cpu_count++;
        if (p.priority != 0) s.has_priorities = true;
    }

    s.avg_burst       = sum / s.process_count;
    s.min_burst       = *std::min_element(bursts.begin(), bursts.end());
    s.max_burst       = *std::max_element(bursts.begin(), bursts.end());
    s.priority_spread = (max_pri == INT_MIN) ? 0 : (max_pri - min_pri);
    s.io_ratio       = static_cast<double>(io_count) / s.process_count;
    s.cpu_ratio      = static_cast<double>(cpu_count) / s.process_count;

    // Variance
    double var = 0.0;
    for (int b : bursts) {
        double diff = b - s.avg_burst;
        var += diff * diff;
    }
    s.burst_variance = var / s.process_count;
    s.burst_stddev   = std::sqrt(s.burst_variance);

    // Median
    std::vector<int> sorted_b = bursts;
    std::sort(sorted_b.begin(), sorted_b.end());
    int mid = s.process_count / 2;
    s.median_burst = (s.process_count % 2 == 0)
                       ? (sorted_b[mid - 1] + sorted_b[mid]) / 2.0
                       : sorted_b[mid];

    // System load: processes arriving per unit time
    int max_arrival = 0;
    for (const auto& p : processes)
        max_arrival = std::max(max_arrival, p.arrival_time);
    s.system_load = (max_arrival > 0)
                      ? static_cast<double>(s.process_count) / max_arrival
                      : static_cast<double>(s.process_count);

    return s;
}

// ─────────────────────────────────────────────
//  Dynamic Round-Robin quantum computation
//  base     = ceil(avg_burst × 0.4)
//  var_adj  = ceil(sqrt(variance) × 0.2)
//  load_adj = n > 8 ? 1 : 0
//  quantum  = clamp(base + var_adj + load_adj, 2, 20)
// ─────────────────────────────────────────────
int compute_dynamic_quantum(const WorkloadStats& stats) {
    int base     = static_cast<int>(std::ceil(stats.avg_burst    * 0.4));
    int var_adj  = static_cast<int>(std::ceil(stats.burst_stddev * 0.2));
    int load_adj = (stats.process_count > 8) ? 1 : 0;
    int q        = base + var_adj + load_adj;
    return std::max(2, std::min(20, q));
}

} // namespace Metrics
