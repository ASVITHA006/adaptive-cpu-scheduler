#include "scheduler.h"
#include "metrics.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <map>
#include <sstream>

// ─────────────────────────────────────────────
//  Helper: push an idle segment onto timeline
// ─────────────────────────────────────────────
static void push_idle(std::vector<TimelineSegment>& tl, int start, int end) {
    if (start < end) {
        tl.push_back({"IDLE", start, end, true, false});
    }
}

// ─────────────────────────────────────────────
//  Helper: apply aging to waiting processes
//  Returns list of aging events that occurred.
// ─────────────────────────────────────────────
static std::vector<AgingEvent> apply_aging(
    std::vector<Process>& procs,
    int current_time,
    int threshold,
    int boost)
{
    std::vector<AgingEvent> events;
    for (auto& p : procs) {
        if (p.is_completed() || p.start_time == current_time) continue;
        int wait = current_time - p.arrival_time - (p.burst_time - p.remaining_time);
        if (wait <= 0) continue;
        if (wait > 0 && wait % threshold == 0) {
            int old_pri = p.effective_priority;
            p.effective_priority = std::max(0, p.effective_priority - boost);
            if (p.effective_priority != old_pri) {
                p.priority_boosts++;
                events.push_back({current_time, p.id, old_pri, p.effective_priority, wait});
            }
        }
    }
    return events;
}

// ─────────────────────────────────────────────
//  Helper: detect starvation among ready procs
// ─────────────────────────────────────────────
static std::vector<StarvationWarning> detect_starvation(
    const std::vector<Process>& procs,
    int current_time,
    int threshold)
{
    std::vector<StarvationWarning> warnings;
    for (const auto& p : procs) {
        if (p.is_completed()) continue;
        if (p.arrival_time > current_time) continue;
        int wait = current_time - p.arrival_time - (p.burst_time - p.remaining_time);
        if (wait >= threshold) {
            warnings.push_back({p.id, current_time, wait});
        }
    }
    return warnings;
}

// ═════════════════════════════════════════════
//  FCFS — First Come First Serve
// ═════════════════════════════════════════════
SimulationResult Algorithms::run_fcfs(std::vector<Process> procs, const SchedulerConfig& cfg) {
    SimulationResult result;
    result.algorithm_name = "FCFS";

    // Sort by arrival time
    std::sort(procs.begin(), procs.end(),
              [](const Process& a, const Process& b){ return a.arrival_time < b.arrival_time; });

    int time = 0;
    for (auto& p : procs) p.reset(p.burst_time);

    for (auto& p : procs) {
        if (time < p.arrival_time) {
            push_idle(result.timeline, time, p.arrival_time);
            time = p.arrival_time;
        }
        p.start_time  = time;
        result.timeline.push_back({p.id, time, time + p.burst_time, false, false});
        time         += p.burst_time;
        p.finish_time = time;
    }

    result.processes = procs;
    Metrics::compute_process_metrics(result);
    return result;
}

// ═════════════════════════════════════════════
//  SJF — Shortest Job First (non-preemptive)
// ═════════════════════════════════════════════
SimulationResult Algorithms::run_sjf(std::vector<Process> procs, const SchedulerConfig& cfg) {
    SimulationResult result;
    result.algorithm_name = "SJF";
    for (auto& p : procs) p.reset(p.burst_time);

    int n = static_cast<int>(procs.size());
    int time = 0, done = 0;

    while (done < n) {
        // Collect ready processes
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].is_completed()) continue;
            if (procs[i].arrival_time > time) continue;
            if (best == -1 || procs[i].burst_time < procs[best].burst_time)
                best = i;
        }

        if (best == -1) {
            // CPU idle: advance to next arrival
            int next = INT_MAX;
            for (auto& p : procs)
                if (!p.is_completed()) next = std::min(next, p.arrival_time);
            push_idle(result.timeline, time, next);
            time = next;
            continue;
        }

        Process& p   = procs[best];
        p.start_time = time;
        result.timeline.push_back({p.id, time, time + p.burst_time, false, false});
        time         += p.burst_time;
        p.finish_time = time;
        p.remaining_time = 0;
        done++;
    }

    result.processes = procs;
    Metrics::compute_process_metrics(result);
    return result;
}

// ═════════════════════════════════════════════
//  SRTF — Shortest Remaining Time First (preemptive)
// ═════════════════════════════════════════════
SimulationResult Algorithms::run_srtf(std::vector<Process> procs, const SchedulerConfig& cfg) {
    SimulationResult result;
    result.algorithm_name = "SRTF";
    for (auto& p : procs) p.reset(p.burst_time);

    int n = static_cast<int>(procs.size());
    int time = 0, done = 0;
    std::string running = "";

    while (done < n) {
        // Find shortest remaining time among ready
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].is_completed()) continue;
            if (procs[i].arrival_time > time) continue;
            if (best == -1 || procs[i].remaining_time < procs[best].remaining_time)
                best = i;
        }

        if (best == -1) {
            int next = INT_MAX;
            for (auto& p : procs)
                if (!p.is_completed()) next = std::min(next, p.arrival_time);
            push_idle(result.timeline, time, next);
            running = "";
            time = next;
            continue;
        }

        Process& p = procs[best];
        if (p.start_time == -1) p.start_time = time;

        // Find next event: either process completes or new shorter arrives
        int next_event = time + p.remaining_time;
        for (const auto& q : procs) {
            if (q.is_completed() || q.arrival_time <= time) continue;
            if (q.arrival_time < next_event) next_event = q.arrival_time;
        }

        int duration = next_event - time;
        if (!result.timeline.empty() && result.timeline.back().process_id == p.id && !result.timeline.back().is_idle)
            result.timeline.back().end = next_event;
        else
            result.timeline.push_back({p.id, time, next_event, false, false});

        p.remaining_time -= duration;
        time              = next_event;

        if (p.remaining_time <= 0) {
            p.finish_time    = time;
            p.remaining_time = 0;
            done++;
        }
    }

    result.processes = procs;
    Metrics::compute_process_metrics(result);
    return result;
}

// ═════════════════════════════════════════════
//  Priority (Non-Preemptive) with Aging
// ═════════════════════════════════════════════
SimulationResult Algorithms::run_priority(std::vector<Process> procs, const SchedulerConfig& cfg) {
    SimulationResult result;
    result.algorithm_name = "Priority";
    for (auto& p : procs) { p.reset(p.burst_time); p.original_priority = p.priority; }

    int n = static_cast<int>(procs.size());
    int time = 0, done = 0;

    while (done < n) {
        // Apply aging to ready-but-not-scheduled processes
        auto events = apply_aging(procs, time, cfg.starv_threshold, cfg.aging_boost);
        result.aging_events.insert(result.aging_events.end(), events.begin(), events.end());

        // Detect starvation
        auto warnings = detect_starvation(procs, time, cfg.starv_threshold);
        result.starvation_warnings.insert(result.starvation_warnings.end(),
                                          warnings.begin(), warnings.end());

        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].is_completed()) continue;
            if (procs[i].arrival_time > time) continue;
            if (best == -1 || procs[i].effective_priority < procs[best].effective_priority)
                best = i;
        }

        if (best == -1) {
            int next = INT_MAX;
            for (auto& p : procs)
                if (!p.is_completed()) next = std::min(next, p.arrival_time);
            push_idle(result.timeline, time, next);
            time = next;
            continue;
        }

        Process& p   = procs[best];
        p.start_time = time;
        result.timeline.push_back({p.id, time, time + p.burst_time, false, false});
        time         += p.burst_time;
        p.finish_time = time;
        p.remaining_time = 0;
        done++;
    }

    result.processes = procs;
    Metrics::compute_process_metrics(result);
    return result;
}

// ═════════════════════════════════════════════
//  Priority Preemptive with Aging
// ═════════════════════════════════════════════
SimulationResult Algorithms::run_priority_preemptive(
    std::vector<Process> procs, const SchedulerConfig& cfg)
{
    SimulationResult result;
    result.algorithm_name = "PriorityP";
    for (auto& p : procs) { p.reset(p.burst_time); p.original_priority = p.priority; }

    int n = static_cast<int>(procs.size());
    int time = 0, done = 0;

    while (done < n) {
        auto events = apply_aging(procs, time, cfg.starv_threshold, cfg.aging_boost);
        result.aging_events.insert(result.aging_events.end(), events.begin(), events.end());

        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].is_completed()) continue;
            if (procs[i].arrival_time > time) continue;
            if (best == -1 || procs[i].effective_priority < procs[best].effective_priority)
                best = i;
        }

        if (best == -1) {
            int next = INT_MAX;
            for (auto& p : procs)
                if (!p.is_completed()) next = std::min(next, p.arrival_time);
            push_idle(result.timeline, time, next);
            time = next;
            continue;
        }

        Process& p = procs[best];
        if (p.start_time == -1) p.start_time = time;

        // Run until preempted or done
        int next_event = time + p.remaining_time;
        for (const auto& q : procs) {
            if (q.is_completed() || q.arrival_time <= time) continue;
            if (q.arrival_time < next_event && q.priority < p.effective_priority)
                next_event = q.arrival_time;
        }
        // Check aging threshold
        int next_age = (cfg.starv_threshold > 0)
                         ? (time + cfg.starv_threshold - (time % cfg.starv_threshold))
                         : next_event;
        next_event = std::min(next_event, next_age);

        int dur = next_event - time;
        if (!result.timeline.empty() && result.timeline.back().process_id == p.id
            && !result.timeline.back().is_idle)
            result.timeline.back().end = next_event;
        else
            result.timeline.push_back({p.id, time, next_event, false, false});

        p.remaining_time -= dur;
        time              = next_event;

        if (p.remaining_time <= 0) {
            p.finish_time    = time;
            p.remaining_time = 0;
            done++;
        }
    }

    result.processes = procs;
    Metrics::compute_process_metrics(result);
    return result;
}

// ═════════════════════════════════════════════
//  Round Robin with Dynamic Quantum
// ═════════════════════════════════════════════
SimulationResult Algorithms::run_rr(
    std::vector<Process> procs, const SchedulerConfig& cfg, int quantum)
{
    SimulationResult result;

    WorkloadStats stats = Metrics::analyse_workload(procs);
    int initial_q = quantum > 0 ? quantum : (cfg.quantum > 0 ? cfg.quantum : 0);
    int final_q   = (initial_q == 0) ? Metrics::compute_dynamic_quantum(stats) : initial_q;

    result.algorithm_name  = "RR";
    result.initial_quantum = initial_q;
    result.final_quantum   = final_q;

    for (auto& p : procs) p.reset(p.burst_time);

    int n = static_cast<int>(procs.size());
    int time = 0, done = 0;
    std::vector<int> ready_queue;
    std::vector<bool> in_queue(n, false);
    std::vector<bool> completed(n, false);

    // Seed with processes arriving at t=0
    for (int i = 0; i < n; i++) {
        if (procs[i].arrival_time == 0) { ready_queue.push_back(i); in_queue[i] = true; }
    }

    while (done < n) {
        if (ready_queue.empty()) {
            // Find next arrival
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!completed[i]) next = std::min(next, procs[i].arrival_time);
            push_idle(result.timeline, time, next);
            time = next;
            for (int i = 0; i < n; i++)
                if (!completed[i] && procs[i].arrival_time <= time && !in_queue[i]) {
                    ready_queue.push_back(i);
                    in_queue[i] = true;
                }
            continue;
        }

        int idx = ready_queue.front();
        ready_queue.erase(ready_queue.begin());
        in_queue[idx] = false;

        Process& p = procs[idx];
        if (p.start_time == -1) p.start_time = time;

        int slice = std::min(final_q, p.remaining_time);
        result.timeline.push_back({p.id, time, time + slice, false, false});
        time              += slice;
        p.remaining_time  -= slice;

        // Enqueue newly arrived processes
        for (int i = 0; i < n; i++) {
            if (!completed[i] && i != idx && procs[i].arrival_time <= time && !in_queue[i]) {
                ready_queue.push_back(i);
                in_queue[i] = true;
            }
        }

        if (p.remaining_time <= 0) {
            p.finish_time = time;
            completed[idx] = true;
            done++;
        } else {
            ready_queue.push_back(idx);
            in_queue[idx] = true;
        }
    }

    result.processes = procs;
    Metrics::compute_process_metrics(result);
    return result;
}

// ═════════════════════════════════════════════
//  Adaptive Decision Engine
// ═════════════════════════════════════════════

namespace AdaptiveEngine {

// Scoring weights for each algorithm per workload characteristic.
// Higher score = better fit.
AdaptiveDecision decide(const std::vector<Process>& procs, const SchedulerConfig& cfg) {
    WorkloadStats s = Metrics::analyse_workload(procs);
    AdaptiveDecision d;
    d.workload_stats = s;

    struct Score { double v; std::string reason; };
    std::map<std::string, Score> scores;

    // ── FCFS: loves uniform, ordered, low-variance workloads ──────────────
    {
        double sc = 50.0;
        std::string r = "baseline";
        if (s.burst_variance < 4.0)   { sc += 30; r += "; uniform burst times"; }
        if (s.system_load < 0.5)      { sc += 10; r += "; light load"; }
        if (s.burst_variance > 25.0)  { sc -= 30; r += "; high variance hurts FCFS"; }
        if (s.has_priorities)         { sc -= 15; r += "; priorities present (ignored by FCFS)"; }
        scores["FCFS"] = {sc, r};
    }

    // ── SJF: loves moderate variance, no real-time requirement ────────────
    {
        double sc = 50.0;
        std::string r = "baseline";
        if (s.burst_variance >= 4.0 && s.burst_variance < 20.0) { sc += 25; r += "; moderate variance"; }
        if (s.system_load < 0.6)      { sc += 15; r += "; low-to-moderate load"; }
        if (s.burst_variance >= 20.0) { sc -= 20; r += "; high variance (SRTF preferred)"; }
        if (s.has_priorities)         { sc -= 10; r += "; priorities present"; }
        scores["SJF"] = {sc, r};
    }

    // ── SRTF: loves high variance, long jobs, preemption capability ───────
    {
        double sc = 45.0;
        std::string r = "baseline";
        if (s.burst_variance >= 20.0)  { sc += 30; r += "; high variance — preemption reduces wait"; }
        if (s.avg_burst > 8.0)         { sc += 20; r += "; long average burst"; }
        if (s.system_load > 0.5)       { sc -= 10; r += "; context switch cost at high load"; }
        if (s.system_load > 0.8)       { sc += 10; r += "; heavy load benefits from preemption"; }
        scores["SRTF"] = {sc, r};
    }

    // ── Priority: loves priority-differentiated workloads ─────────────────
    {
        double sc = 40.0;
        std::string r = "baseline";
        if (s.priority_spread >= 4)    { sc += 35; r += "; wide priority spread — critical precedence"; }
        if (s.priority_spread >= 2)    { sc += 15; r += "; priority differentiation present"; }
        if (!s.has_priorities)         { sc -= 40; r += "; no priorities — wasted"; }
        if (s.burst_variance > 20.0)   { sc -= 10; r += "; high variance may cause starvation"; }
        scores["Priority"] = {sc, r};
    }

    // ── Round Robin: loves interactive/mixed/heavy-load workloads ─────────
    {
        double sc = 45.0;
        std::string r = "baseline";
        if (s.system_load > 0.6)       { sc += 20; r += "; heavy load — RR ensures fairness"; }
        if (s.io_ratio > 0.3)          { sc += 15; r += "; IO-bound mix — time-sharing beneficial"; }
        if (s.process_count > 6)       { sc += 10; r += "; many processes — RR prevents monopoly"; }
        if (s.burst_variance < 4.0)    { sc -= 15; r += "; uniform bursts — RR overhead not justified"; }
        scores["RR"] = {sc, r};
    }

    // Find winner
    std::string best_algo = "FCFS";
    double best_score = -1e9;
    for (auto& [algo, score] : scores) {
        if (score.v > best_score) { best_score = score.v; best_algo = algo; }
    }

    // Build scoring table for output
    for (auto& [algo, score] : scores) {
        AdaptiveDecision::CandidateScore cs;
        cs.algorithm = algo; cs.score = score.v; cs.reason = score.reason;
        d.scoring_table.push_back(cs);
    }
    std::sort(d.scoring_table.begin(), d.scoring_table.end(),
              [](const auto& a, const auto& b){ return a.score > b.score; });

    d.chosen_algorithm = best_algo;

    // Human-readable reasoning
    std::ostringstream oss;
    oss << "Selected " << best_algo << " (score=" << best_score << "). ";
    oss << "Workload: avg_burst=" << s.avg_burst
        << ", variance=" << s.burst_variance
        << ", load=" << s.system_load
        << ", priority_spread=" << s.priority_spread
        << ". Reason: " << scores[best_algo].reason;
    d.reasoning = oss.str();

    return d;
}

SimulationResult run_adaptive(std::vector<Process> procs, const SchedulerConfig& cfg) {
    AdaptiveDecision decision = decide(procs, cfg);
    const std::string& algo   = decision.chosen_algorithm;

    SimulationResult result;
    if      (algo == "FCFS")     result = Algorithms::run_fcfs(procs, cfg);
    else if (algo == "SJF")      result = Algorithms::run_sjf(procs, cfg);
    else if (algo == "SRTF")     result = Algorithms::run_srtf(procs, cfg);
    else if (algo == "Priority")  result = Algorithms::run_priority(procs, cfg);
    else if (algo == "PriorityP") result = Algorithms::run_priority_preemptive(procs, cfg);
    else                          result = Algorithms::run_rr(procs, cfg);

    result.algorithm_name = "Adaptive → " + algo;
    return result;
}

} // namespace AdaptiveEngine

// ═════════════════════════════════════════════
//  Comparison Mode
// ═════════════════════════════════════════════
namespace Comparison {

ComparisonReport run_all(std::vector<Process> procs, const SchedulerConfig& cfg) {
    ComparisonReport report;
    report.adaptive_decision = AdaptiveEngine::decide(procs, cfg);

    // Run every algorithm
    report.results.push_back(Algorithms::run_fcfs(procs, cfg));
    report.results.push_back(Algorithms::run_sjf(procs, cfg));
    report.results.push_back(Algorithms::run_srtf(procs, cfg));
    report.results.push_back(Algorithms::run_priority(procs, cfg));
    report.results.push_back(Algorithms::run_priority_preemptive(procs, cfg));
    report.results.push_back(Algorithms::run_rr(procs, cfg));

    // Find bests
    auto best_by = [&](auto metric_fn) -> std::string {
        std::string best;
        double best_val = 1e18;
        for (const auto& r : report.results) {
            double v = metric_fn(r);
            if (v < best_val) { best_val = v; best = r.algorithm_name; }
        }
        return best;
    };

    report.lowest_wait    = best_by([](const SimulationResult& r){ return r.avg_waiting_time; });
    report.fewest_switches = best_by([](const SimulationResult& r){ return (double)r.context_switches; });

    // Best fairness = highest Jain's index
    double best_fi = -1;
    for (const auto& r : report.results) {
        if (r.fairness_index > best_fi) { best_fi = r.fairness_index; report.best_fairness = r.algorithm_name; }
    }

    // Best throughput = highest value
    double best_tp = -1;
    for (const auto& r : report.results) {
        if (r.throughput > best_tp) { best_tp = r.throughput; report.best_throughput = r.algorithm_name; }
    }

    // Weighted overall score: lower wait(40%) + lower tat(30%) + lower switches(15%) + higher fairness(15%)
    double best_ws = 1e18;
    for (const auto& r : report.results) {
        double ws = 0.40 * r.avg_waiting_time
                  + 0.30 * r.avg_turnaround_time
                  + 0.15 * r.context_switches
                  - 0.15 * r.fairness_index * 100;
        if (ws < best_ws) { best_ws = ws; report.best_overall = r.algorithm_name; }
    }

    return report;
}

} // namespace Comparison

// ═════════════════════════════════════════════
//  Unified Entry Point
// ═════════════════════════════════════════════
SimulationResult run_scheduler(std::vector<Process> procs, const SchedulerConfig& cfg) {
    const std::string& algo = cfg.algorithm;

    if (algo == "FCFS")     return Algorithms::run_fcfs(procs, cfg);
    if (algo == "SJF")      return Algorithms::run_sjf(procs, cfg);
    if (algo == "SRTF")     return Algorithms::run_srtf(procs, cfg);
    if (algo == "Priority")  return Algorithms::run_priority(procs, cfg);
    if (algo == "PriorityP") return Algorithms::run_priority_preemptive(procs, cfg);
    if (algo == "RR")        return Algorithms::run_rr(procs, cfg);
    return AdaptiveEngine::run_adaptive(procs, cfg);
}
