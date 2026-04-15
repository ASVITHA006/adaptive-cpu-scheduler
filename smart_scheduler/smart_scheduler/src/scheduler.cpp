#include "../include/scheduler.h"
#include <algorithm>
#include <queue>
#include <cmath>
#include <climits>
#include <numeric>

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void finalize_stats(std::vector<Process>& procs,
                            const std::vector<TimelineSegment>& tl) {
    for (auto& seg : tl) {
        if (seg.pid == "IDLE") continue;
        for (auto& p : procs) {
            if (p.id != seg.pid) continue;
            if (p.first_run_time == -1) p.first_run_time = seg.start;
            p.completion_time = std::max(p.completion_time, seg.end);
        }
    }
}

static int count_idle(const std::vector<TimelineSegment>& tl) {
    int idle = 0;
    for (auto& s : tl) if (s.pid == "IDLE") idle += s.end - s.start;
    return idle;
}

static int total_time_of(const std::vector<TimelineSegment>& tl) {
    int t = 0;
    for (auto& s : tl) t = std::max(t, s.end);
    return t;
}

// Merge consecutive same-PID segments
static std::vector<TimelineSegment> merge(std::vector<TimelineSegment> tl) {
    std::vector<TimelineSegment> out;
    for (auto& seg : tl) {
        if (!out.empty() && out.back().pid == seg.pid && out.back().end == seg.start)
            out.back().end = seg.end;
        else
            out.push_back(seg);
    }
    return out;
}

// Compute workload statistics used by both adaptive engine and dynamic quantum
static void compute_workload_stats(const std::vector<Process>& procs,
                                    double& avg, double& var,
                                    int& pmin, int& pmax,
                                    double& total_burst) {
    total_burst = 0;
    pmin = INT_MAX; pmax = INT_MIN;
    for (auto& p : procs) {
        total_burst += p.burst_time;
        pmin = std::min(pmin, p.priority);
        pmax = std::max(pmax, p.priority);
    }
    avg = total_burst / procs.size();
    var = 0;
    for (auto& p : procs) var += (p.burst_time - avg) * (p.burst_time - avg);
    var /= procs.size();
}

std::string algorithm_name(Algorithm a) {
    switch (a) {
        case Algorithm::FCFS:                return "FCFS";
        case Algorithm::SJF:                 return "SJF";
        case Algorithm::SRTF:                return "SRTF";
        case Algorithm::PRIORITY:            return "Priority";
        case Algorithm::PRIORITY_PREEMPTIVE: return "Priority (Preemptive)";
        case Algorithm::ROUND_ROBIN:         return "Round Robin";
        case Algorithm::ADAPTIVE:            return "Adaptive";
    }
    return "Unknown";
}

// ─── Dynamic Time Quantum Adjustment Module ───────────────────────────────────
//
// Computes optimal RR quantum from workload statistics to:
//  - reduce excessive context switching (don't go too small)
//  - improve responsiveness (don't go too large)
//
// Formula:
//   base     = ceil(avg_burst * 0.4)          — 40% of avg burst
//   var_adj  = ceil(sqrt(variance) * 0.2)     — variance penalty: high spread → bigger quantum
//   load_adj = n > 8 ? +1 : 0                 — heavy load → slightly larger quantum
//   quantum  = clamp(base + var_adj + load_adj, 2, 20)
//
int compute_dynamic_quantum(const std::vector<Process>& procs) {
    if (procs.empty()) return 4;
    double avg, var, total_burst;
    int pmin, pmax;
    compute_workload_stats(procs, avg, var, pmin, pmax, total_burst);
    int base     = static_cast<int>(std::ceil(avg * 0.4));
    int var_adj  = static_cast<int>(std::ceil(std::sqrt(var) * 0.2));
    int load_adj = (static_cast<int>(procs.size()) > 8) ? 1 : 0;
    return std::max(2, std::min(20, base + var_adj + load_adj));
}

// ─── Adaptive Decision Engine ─────────────────────────────────────────────────
//
// Inputs used (per abstract):
//   1. avg_burst          — average burst time
//   2. burst_variance     — burst time variance
//   3. waiting_threshold  — configurable starvation / waiting time threshold
//   4. system_load        — processes per time unit (n / total_expected_time)
//   5. context_switch_rate — estimated CS frequency (n / total_expected_time if RR chosen)
//
AdaptiveDecision adaptive_select(const std::vector<Process>& procs,
                                  int starv_threshold) {
    if (procs.empty()) return {Algorithm::FCFS, "No processes.", 4,
                                0, 0, 0, 0, 0, (double)starv_threshold};

    double avg, var, total_burst;
    int pmin, pmax;
    compute_workload_stats(procs, avg, var, pmin, pmax, total_burst);
    int pspread = pmax - pmin;
    int n       = static_cast<int>(procs.size());

    // System load: number of processes relative to estimated total execution span
    int latest_arrival = 0;
    for (auto& p : procs) latest_arrival = std::max(latest_arrival, p.arrival_time);
    double estimated_span = total_burst + latest_arrival;
    double system_load    = (estimated_span > 0) ? (double)n / estimated_span : 1.0;

    // Estimated context switch rate if RR were chosen
    int dyn_quantum = compute_dynamic_quantum(procs);
    double est_cs_if_rr = (dyn_quantum > 0) ? total_burst / dyn_quantum : 0;
    double est_cs_rate  = (estimated_span > 0) ? est_cs_if_rr / estimated_span : 0;

    // Effective waiting time threshold (used in decision: high load → tighten threshold)
    double effective_wait_threshold = (system_load > 0.5)
                                        ? starv_threshold * 0.75
                                        : starv_threshold;

    AdaptiveDecision d;
    d.avg_burst              = avg;
    d.burst_variance         = var;
    d.priority_spread        = pspread;
    d.quantum                = dyn_quantum;
    d.system_load            = system_load;
    d.context_switch_rate    = est_cs_rate;
    d.waiting_time_threshold = effective_wait_threshold;

    // Decision logic — uses all 5 workload characteristics
    if (var < 4.0) {
        // Very uniform workload → FCFS has zero overhead
        d.algo   = Algorithm::FCFS;
        d.reason = "Uniform burst times (variance=" +
                    std::to_string(var).substr(0,4) +
                   ", load=" + std::to_string(system_load).substr(0,4) +
                   "). FCFS minimises overhead with no starvation risk.";
    } else if (pspread >= 4 && effective_wait_threshold <= starv_threshold) {
        // Wide priority spread + manageable waiting threshold → Priority scheduling
        d.algo   = Algorithm::PRIORITY;
        d.reason = "Wide priority spread (" + std::to_string(pspread) +
                   " levels, wait_threshold=" +
                   std::to_string((int)effective_wait_threshold) +
                   "). Priority scheduling with aging ensures critical processes run first.";
    } else if (var < 20.0 && system_load < 0.6) {
        // Moderate variance, low system load → SJF minimises avg waiting
        d.algo   = Algorithm::SJF;
        d.reason = "Moderate variance (" + std::to_string(var).substr(0,5) +
                   "), low system load (" + std::to_string(system_load).substr(0,4) +
                   "). SJF minimises average waiting time.";
    } else if (var >= 20.0 && avg > 8.0) {
        // High variance + long bursts → preemption needed
        d.algo   = Algorithm::SRTF;
        d.reason = "High variance (" + std::to_string(var).substr(0,5) +
                   "), long avg burst (" + std::to_string(avg).substr(0,5) +
                   "). SRTF preempts to minimise turnaround time.";
    } else if (est_cs_rate < 1.5 || system_load >= 0.6) {
        // Mixed workload or high load → RR with dynamic quantum ensures fairness
        d.algo   = Algorithm::ROUND_ROBIN;
        d.reason = "Mixed workload (n=" + std::to_string(n) +
                   ", load=" + std::to_string(system_load).substr(0,4) +
                   "). Round Robin with dynamic quantum=" +
                   std::to_string(dyn_quantum) +
                   " balances fairness and context-switch overhead (est. rate=" +
                   std::to_string(est_cs_rate).substr(0,4) + "/u).";
    } else {
        d.algo   = Algorithm::ROUND_ROBIN;
        d.reason = "Fallback: Round Robin (quantum=" + std::to_string(dyn_quantum) +
                   ") for general mixed workload fairness.";
    }
    return d;
}

// ─── FCFS ─────────────────────────────────────────────────────────────────────

SimulationResult run_fcfs(std::vector<Process> procs) {
    std::sort(procs.begin(), procs.end(),
              [](auto& a, auto& b){ return a.arrival_time < b.arrival_time ||
                                           (a.arrival_time == b.arrival_time && a.id < b.id); });
    std::vector<TimelineSegment> tl;
    int t = 0, cs = 0;
    for (auto& p : procs) {
        if (t < p.arrival_time) { tl.push_back({"IDLE", t, p.arrival_time}); t = p.arrival_time; }
        tl.push_back({p.id, t, t + p.burst_time});
        t += p.burst_time; cs++;
    }
    finalize_stats(procs, tl);
    int tt = total_time_of(tl);
    SimulationResult r{"FCFS", 0, tl, procs, {}, cs - 1, count_idle(tl), tt};
    r.system_load         = (tt > 0) ? (double)procs.size() / tt : 0;
    r.context_switch_rate = (tt > 0) ? (double)(cs - 1) / tt : 0;
    return r;
}

// ─── SJF (non-preemptive) ─────────────────────────────────────────────────────

SimulationResult run_sjf(std::vector<Process> procs) {
    std::vector<TimelineSegment> tl;
    int t = 0, done = 0, n = procs.size(), cs = 0;
    while (done < n) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (!procs[i].completed && procs[i].arrival_time <= t) {
                if (best == -1 || procs[i].burst_time < procs[best].burst_time ||
                    (procs[i].burst_time == procs[best].burst_time &&
                     procs[i].arrival_time < procs[best].arrival_time))
                    best = i;
            }
        }
        if (best == -1) {
            int next = INT_MAX;
            for (auto& p : procs) if (!p.completed) next = std::min(next, p.arrival_time);
            tl.push_back({"IDLE", t, next}); t = next; continue;
        }
        auto& p = procs[best];
        tl.push_back({p.id, t, t + p.burst_time});
        t += p.burst_time; p.completed = true; done++; cs++;
    }
    finalize_stats(procs, tl);
    int tt = total_time_of(tl);
    SimulationResult r{"SJF", 0, tl, procs, {}, cs - 1, count_idle(tl), tt};
    r.system_load         = (tt > 0) ? (double)procs.size() / tt : 0;
    r.context_switch_rate = (tt > 0) ? (double)(cs - 1) / tt : 0;
    return r;
}

// ─── SRTF (preemptive SJF) ────────────────────────────────────────────────────

SimulationResult run_srtf(std::vector<Process> procs) {
    for (auto& p : procs) p.remaining_time = p.burst_time;
    std::vector<TimelineSegment> tl;
    int n = procs.size(), done = 0, cs = 0;
    int max_t = 0;
    for (auto& p : procs) max_t += p.burst_time + p.arrival_time + 1;

    std::string last_pid = "";
    for (int t = 0; t < max_t && done < n; t++) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (!procs[i].completed && procs[i].arrival_time <= t) {
                if (best == -1 || procs[i].remaining_time < procs[best].remaining_time ||
                    (procs[i].remaining_time == procs[best].remaining_time &&
                     procs[i].arrival_time < procs[best].arrival_time))
                    best = i;
            }
        }
        if (best == -1) {
            if (last_pid != "IDLE") { tl.push_back({"IDLE", t, t + 1}); last_pid = "IDLE"; }
            else tl.back().end++;
            continue;
        }
        auto& p = procs[best];
        if (p.id != last_pid) {
            if (!last_pid.empty() && last_pid != "IDLE") cs++;
            tl.push_back({p.id, t, t + 1});
            last_pid = p.id;
        } else { tl.back().end++; }
        p.remaining_time--;
        if (p.remaining_time == 0) { p.completed = true; done++; }
    }
    finalize_stats(procs, tl);
    int tt = total_time_of(tl);
    auto merged = merge(tl);
    SimulationResult r{"SRTF", 0, merged, procs, {}, cs, count_idle(tl), tt};
    r.system_load         = (tt > 0) ? (double)procs.size() / tt : 0;
    r.context_switch_rate = (tt > 0) ? (double)cs / tt : 0;
    return r;
}

// ─── Priority (with starvation detection + aging mechanism) ───────────────────
//
// Per abstract:
//  - Starvation detection: configurable waiting time threshold
//  - Aging mechanism: gradually increases priority of long-waiting processes
//  - Aging boost: priority boost per threshold cycle (prevents indefinite postponement)
//

SimulationResult run_priority(std::vector<Process> procs, bool preemptive,
                               int starv_threshold, int aging_boost) {
    for (auto& p : procs) {
        p.remaining_time     = p.burst_time;
        p.effective_priority = p.priority;
        p.wait_since         = p.arrival_time;
    }
    std::vector<TimelineSegment> tl;
    std::vector<AgingEvent> aging_events;
    int n = procs.size(), done = 0, cs = 0;
    int max_t = 0;
    for (auto& p : procs) max_t += p.burst_time + p.arrival_time + 1;

    std::string last_pid = "";
    for (int t = 0; t < max_t && done < n; t++) {
        // Starvation detection + aging mechanism
        for (auto& p : procs) {
            if (p.completed || p.arrival_time > t) continue;
            int waited = t - p.wait_since;
            // Only boost if process hasn't been running (is waiting)
            if (last_pid == p.id) continue;
            int boosts  = waited / starv_threshold;
            int new_pri = std::max(1, p.priority - boosts * aging_boost);
            if (new_pri < p.effective_priority) {
                aging_events.push_back({p.id, t, p.effective_priority, new_pri, waited});
                p.effective_priority = new_pri;
            }
        }
        // Pick best available process by effective priority
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].completed || procs[i].arrival_time > t) continue;
            if (best == -1 ||
                procs[i].effective_priority < procs[best].effective_priority ||
                (procs[i].effective_priority == procs[best].effective_priority &&
                 procs[i].arrival_time < procs[best].arrival_time))
                best = i;
        }
        if (best == -1) {
            if (last_pid != "IDLE") { tl.push_back({"IDLE", t, t+1}); last_pid = "IDLE"; }
            else tl.back().end++;
            continue;
        }
        auto& p = procs[best];
        if (!preemptive) {
            if (p.id != last_pid) { if (!last_pid.empty() && last_pid != "IDLE") cs++; }
            tl.push_back({p.id, t, t + p.remaining_time});
            t += p.remaining_time - 1;
            p.remaining_time = 0; p.completed = true; done++; cs++;
            last_pid = p.id;
        } else {
            if (p.id != last_pid) {
                if (!last_pid.empty() && last_pid != "IDLE") cs++;
                tl.push_back({p.id, t, t+1});
                last_pid = p.id;
            } else tl.back().end++;
            p.remaining_time--;
            p.wait_since = t;
            if (p.remaining_time == 0) { p.completed = true; done++; }
        }
    }
    std::string algo = preemptive ? "Priority (Preemptive)" : "Priority";
    finalize_stats(procs, tl);
    int tt = total_time_of(tl);
    auto merged = merge(tl);
    SimulationResult r{algo, 0, merged, procs, aging_events, cs, count_idle(tl), tt};
    r.system_load         = (tt > 0) ? (double)procs.size() / tt : 0;
    r.context_switch_rate = (tt > 0) ? (double)cs / tt : 0;
    return r;
}

// ─── Round Robin (with dynamic time quantum) ──────────────────────────────────
//
// Per abstract: dynamic quantum is computed by compute_dynamic_quantum()
// which uses avg_burst, burst_variance, and system load to reduce
// excessive context switching while maintaining responsiveness.
//

SimulationResult run_round_robin(std::vector<Process> procs, int quantum) {
    // If quantum is auto (-1 or 0), compute dynamically from workload statistics
    if (quantum <= 0) quantum = compute_dynamic_quantum(procs);

    for (auto& p : procs) p.remaining_time = p.burst_time;
    std::sort(procs.begin(), procs.end(),
              [](auto& a, auto& b){ return a.arrival_time < b.arrival_time; });

    std::vector<TimelineSegment> tl;
    std::queue<int> q;
    int t = 0, done = 0, n = procs.size(), cs = 0, idx = 0;

    while (idx < n && procs[idx].arrival_time <= t) q.push(idx++);
    if (q.empty() && idx < n) { t = procs[0].arrival_time; q.push(idx++); }

    while (done < n) {
        if (q.empty()) {
            tl.push_back({"IDLE", t, procs[idx].arrival_time});
            t = procs[idx].arrival_time;
            while (idx < n && procs[idx].arrival_time <= t) q.push(idx++);
            continue;
        }
        int i = q.front(); q.pop(); cs++;
        auto& p = procs[i];
        int exec = std::min(quantum, p.remaining_time);
        tl.push_back({p.id, t, t + exec});
        t += exec; p.remaining_time -= exec;
        while (idx < n && procs[idx].arrival_time <= t) q.push(idx++);
        if (p.remaining_time > 0) q.push(i);
        else { p.completed = true; done++; }
    }
    finalize_stats(procs, tl);
    int tt = total_time_of(tl);
    SimulationResult r{"Round Robin", quantum, tl, procs, {}, cs - 1, count_idle(tl), tt};
    r.system_load         = (tt > 0) ? (double)procs.size() / tt : 0;
    r.context_switch_rate = (tt > 0) ? (double)(cs - 1) / tt : 0;
    return r;
}

// ─── Adaptive ─────────────────────────────────────────────────────────────────

SimulationResult run_adaptive(std::vector<Process> procs,
                               int starv_threshold, int aging_boost) {
    auto dec = adaptive_select(procs, starv_threshold);
    SimulationResult r;
    switch (dec.algo) {
        case Algorithm::FCFS:
            r = run_fcfs(procs); break;
        case Algorithm::SJF:
            r = run_sjf(procs); break;
        case Algorithm::SRTF:
            r = run_srtf(procs); break;
        case Algorithm::PRIORITY:
            r = run_priority(procs, false, starv_threshold, aging_boost); break;
        case Algorithm::ROUND_ROBIN:
            r = run_round_robin(procs, dec.quantum); break;
        default:
            r = run_fcfs(procs);
    }
    r.algorithm = "Adaptive \u2192 " + r.algorithm;
    return r;
}
