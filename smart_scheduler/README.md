# Smart Scheduler — Adaptive CPU Scheduling Framework

> A research-grade CPU scheduling simulator with intelligent algorithm selection,
> real-time performance analytics, and a professional web dashboard.

---

## Why This Project Is Different

Most scheduling simulators implement the algorithms and stop there.  
Smart Scheduler goes further:

| Feature | Typical Student Project | Smart Scheduler |
|---|---|---|
| Algorithm count | 1–3 | 6 fully implemented |
| Selection | Manual only | **Scoring-based Adaptive Engine** |
| Explainability | None | **Full decision log + scoring table** |
| Fairness | None | **Jain's Fairness Index** |
| Starvation | None | **Detection + aging log** |
| Round Robin | Fixed quantum | **Dynamic quantum computation** |
| Input | Hardcoded | **JSON config + 6 workload presets** |
| Output | stdout dump | **Animated Gantt + metric charts** |
| Comparison | None | **Run all 6 → ranked report** |
| Architecture | Monolith | **Modular C++17 with clean namespace separation** |

---

## Architecture

```
smart_scheduler/
├── include/
│   ├── process.h          ← Core structs (Process, Timeline, SimulationResult, AgingEvent)
│   ├── metrics.h          ← WorkloadStats, Metrics namespace interface
│   ├── scheduler.h        ← Algorithm declarations, AdaptiveDecision, ComparisonReport
│   └── io_utils.h         ← I/O interface (JSON, tables, workload generator)
│
├── src/
│   ├── main.cpp           ← CLI entry point, argument parsing
│   ├── scheduler.cpp      ← 6 algorithms + Adaptive Engine + Comparison mode
│   ├── metrics.cpp        ← Jain's Fairness, dynamic quantum, workload analysis
│   └── io_utils.cpp       ← JSON serializer, workload generator (6 presets), CLI tables
│
├── gui/
│   ├── app.py             ← Flask bridge (POST /api/run  POST /api/compare)
│   └── templates/
│       └── index.html     ← Dashboard: Gantt · Metrics · Comparison · Decision Engine
│
├── config/
│   └── processes.json     ← Default workload configuration
│
├── Makefile               ← C++17 build
└── run.sh                 ← One-command build + launch
```

### Module Responsibilities

```
┌─────────────┐    JSON     ┌──────────────┐   HTTP    ┌──────────────┐
│  CLI / GUI  │ ──────────► │  C++ Binary  │ ◄──────── │  Flask GUI   │
└─────────────┘             └──────┬───────┘           └──────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              ▼                    ▼                     ▼
       ┌────────────┐    ┌──────────────────┐    ┌───────────┐
       │ Algorithms │    │ Adaptive Engine  │    │  Metrics  │
       │ FCFS SJF   │    │ Scoring system   │    │ Jain's FI │
       │ SRTF Pri   │    │ Workload analyst │    │ Dynamic Q │
       │ PriP RR    │    │ Decision log     │    │ Analytics │
       └────────────┘    └──────────────────┘    └───────────┘
```

---

## Quick Start

```bash
# Build C++ + launch web GUI (one command)
bash run.sh

# Open: http://localhost:5000
```

---

## Manual Usage

```bash
# Build C++ binary
make

# Run individual algorithms
./build/scheduler --algo adaptive
./build/scheduler --algo compare
./build/scheduler --algo RR --quantum 4
./build/scheduler --algo Priority --starv-threshold 10 --aging-boost 2

# Use workload presets
./build/scheduler --generate bursty --algo compare
./build/scheduler --generate heavy  --algo SRTF --verbose

# Custom JSON config
./build/scheduler --config config/processes.json --algo adaptive

# Write output to file
./build/scheduler --algo compare --output results.json

# Start GUI separately
cd gui && pip install flask && python3 app.py
```

---

## CLI Reference

| Flag | Description | Default |
|---|---|---|
| `--algo <name>` | `FCFS` `SJF` `SRTF` `Priority` `PriorityP` `RR` `adaptive` `compare` | `adaptive` |
| `--config <file>` | JSON process config file | `config/processes.json` |
| `--quantum <n>` | RR time quantum (0 = auto-compute from workload) | `0` |
| `--starv-threshold <n>` | Waiting time threshold for starvation detection | `15` |
| `--aging-boost <n>` | Priority boost applied per threshold cycle | `1` |
| `--generate <preset>` | Generate workload: `random` `heavy` `light` `bursty` `uniform` `edge` | — |
| `--seed <n>` | RNG seed for `--generate` | `42` |
| `--output <file>` | Write JSON result to file instead of stdout | stdout |
| `--verbose` | Print formatted tables to stderr | off |
| `--help` | Show help | — |

---

## Algorithms

| Algorithm | Preemptive | Optimal For |
|---|---|---|
| **FCFS** | No | Uniform burst times, batch workloads |
| **SJF** | No | Moderate variance, minimising average wait |
| **SRTF** | Yes | High variance, long-running processes |
| **Priority** | No | Differentiated criticality, real-time tasks |
| **Priority (P)** | Yes | Critical tasks + starvation prevention |
| **Round Robin** | Yes | Interactive, fair time-sharing, mixed load |
| **Adaptive** | — | Unknown workloads — auto-selects the best |

---

## Adaptive Decision Engine

The engine analyses 6 workload dimensions before choosing an algorithm:

| Dimension | Description |
|---|---|
| `avg_burst` | Average burst time across all processes |
| `burst_variance` | Statistical variance of burst times |
| `priority_spread` | Range of priority values (max − min) |
| `system_load` | Processes arriving per time unit |
| `io_ratio` | Fraction of IO-bound processes |
| `process_count` | Total number of processes |

### Scoring System

Each algorithm receives a score (0–100+) based on how well its characteristics match the workload:

```
FCFS:     variance < 4 → +30   |  high variance → −30  |  priorities present → −15
SJF:      moderate variance → +25  |  low load → +15   |  high variance → −20
SRTF:     high variance → +30  |  long avg burst → +20  |  heavy load → +10
Priority: wide priority spread → +35  |  no priorities → −40
RR:       heavy load → +20     |  high IO ratio → +15   |  many processes → +10
```

The algorithm with the highest score is selected. The full scoring table is included in the JSON output and shown in the GUI dashboard.

### Decision Output (JSON)

```json
{
  "adaptive_decision": {
    "chosen": "SRTF",
    "reasoning": "Selected SRTF (score=95). Workload: avg_burst=14.5, variance=82.3, load=2.0. Reason: high variance — preemption reduces wait; long average burst",
    "scoring_table": [
      {"algorithm": "SRTF",     "score": 95.0, "reason": "high variance — preemption reduces wait; long average burst"},
      {"algorithm": "Priority", "score": 75.0, "reason": "wide priority spread — critical precedence"},
      {"algorithm": "RR",       "score": 65.0, "reason": "heavy load — RR ensures fairness"},
      {"algorithm": "SJF",      "score": 30.0, "reason": "high variance (SRTF preferred)"},
      {"algorithm": "FCFS",     "score": 20.0, "reason": "high variance hurts FCFS"}
    ],
    "workload": {
      "avg_burst": 14.5, "burst_variance": 82.3,
      "system_load": 2.0, "priority_spread": 3,
      "process_count": 6, "io_ratio": 0.17
    }
  }
}
```

---

## Dynamic Round Robin Quantum

When `--quantum 0` (the default), the quantum is computed automatically:

```
base     = ⌈avg_burst × 0.4⌉
var_adj  = ⌈√variance × 0.2⌉     # high variance → larger quantum
load_adj = n > 8 ? 1 : 0          # heavy load → slightly larger quantum
quantum  = clamp(base + var_adj + load_adj, 2, 20)
```

This formula reduces excessive context switching on variable workloads while maintaining interactive responsiveness. Both the computed and requested quantum values are reported in the output.

---

## Starvation Detection & Aging

- A process is marked **starved** when its waiting time exceeds `--starv-threshold` (default: 15 units)
- Each threshold cycle, its `effective_priority` is boosted by `--aging-boost`
- All events are timestamped and included in `aging_events[]` and `starvation_warnings[]`
- The GUI dashboard displays a per-event log with old/new priority and accumulated wait time

---

## Workload Presets

| Preset | Processes | Characteristics |
|---|---|---|
| `random` | 8 | Mixed IO/CPU, random burst 1–20, random priority |
| `heavy` | 12 | All CPU-bound, burst 10–30, dense arrivals |
| `light` | 5 | IO-bound, burst 1–5, staggered arrivals |
| `bursty` | 6 | Extreme variance (burst 1–30), mixed types |
| `uniform` | 6 | All burst=5, CPU-bound — tests FCFS vs RR |
| `edge` | 3 | Identical arrivals, late process — stress tests |

---

## Performance Metrics

| Metric | Formula | Notes |
|---|---|---|
| Waiting time | TAT − burst_time | Time in ready queue |
| Turnaround time | finish − arrival | Total lifecycle |
| Response time | first_CPU − arrival | Latency to first dispatch |
| Throughput | n / makespan | Processes per time unit |
| CPU utilization | (makespan − idle) / makespan × 100 | Efficiency |
| Context switches | Count of preemptions/dispatches | Overhead indicator |
| **Jain's Fairness Index** | (Σwᵢ)² / (n × Σwᵢ²) | 1.0 = perfect fairness |

---

## Config File Format

```json
{
  "starvation_threshold": 15,
  "aging_boost": 1,
  "processes": [
    {"id": "P1", "arrival": 0, "burst": 6,  "priority": 2, "type": "CPU"},
    {"id": "P2", "arrival": 1, "burst": 4,  "priority": 1, "type": "IO"},
    {"id": "P3", "arrival": 2, "burst": 8,  "priority": 3, "type": "MIXED"}
  ]
}
```

`type` values: `CPU` (default), `IO`, `MIXED`

---

## Dependencies

| Component | Requirement |
|---|---|
| C++ compiler | g++ with C++17 (`-std=c++17`) |
| Build tool | GNU Make |
| Python | 3.8+ |
| Flask | `pip install flask` |
| Browser | Any modern browser (Chrome, Firefox, Safari) |

```bash
# Ubuntu/Debian
sudo apt install g++ make python3 python3-pip

# macOS (Homebrew)
brew install gcc make python3
```

---

## Authors

- **Asvitha S** (24PD06)  
- **Harshini M S K** (24PD16)

*Department of Computer Science — Operating Systems Project*
