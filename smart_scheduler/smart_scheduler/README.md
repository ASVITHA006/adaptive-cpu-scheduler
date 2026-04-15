# Smart Scheduler — Adaptive CPU Scheduling Framework

An advanced adaptive CPU scheduling framework that dynamically selects and adjusts
scheduling strategies based on real-time workload characteristics.

## Project Structure

```
smart_scheduler/
├── src/
│   ├── main.cpp          # CLI entry point & argument parsing
│   ├── scheduler.cpp     # 6 algorithms + adaptive engine + dynamic quantum
│   ├── io_utils.cpp      # JSON config loader & JSON output serialiser
│   └── process.cpp       # Process struct methods & performance metrics
├── include/
│   ├── process.h         # Process, TimelineSegment, SimulationResult structs
│   ├── scheduler.h       # Algorithm declarations & AdaptiveDecision struct
│   └── io_utils.h        # I/O function declarations
├── gui/
│   ├── app.py            # Flask bridge (calls C++ binary, serves web GUI)
│   ├── requirements.txt
│   └── templates/
│       └── index.html    # Web GUI: Gantt, metrics, fairness, aging, decision engine
├── config/
│   └── processes.json    # Default process configuration
├── Makefile
└── run.sh                # One-command build + launch
```

## Quick Start

```bash
# Build C++ + launch web GUI (one command)
bash run.sh

# Then open: http://localhost:5000
```

## Manual Steps

```bash
# 1. Build C++ binary
make

# 2. Run C++ directly (outputs JSON to stdout)
./build/scheduler --algo adaptive
./build/scheduler --algo RR                        # quantum auto-computed
./build/scheduler --algo RR --quantum 4            # manual quantum
./build/scheduler --algo Priority --starv-threshold 10 --aging-boost 2
./build/scheduler --config config/processes.json --algo FCFS

# 3. Start GUI separately
cd gui
pip install flask
python3 app.py
```

## CLI Options

| Flag | Description | Default |
|---|---|---|
| `--config <file>` | JSON config file | `config/processes.json` |
| `--algo <name>` | FCFS, SJF, SRTF, Priority, PriorityP, RR, adaptive | `adaptive` |
| `--quantum <n>` | RR time quantum (0 = auto-computed from workload) | auto |
| `--starv-threshold <n>` | Waiting time threshold for starvation detection | 15 |
| `--aging-boost <n>` | Priority boost per threshold cycle | 1 |
| `--output <file>` | Write JSON to file instead of stdout | stdout |

## Config File Format

```json
{
  "starvation_threshold": 15,
  "aging_boost": 1,
  "processes": [
    {"id": "P1", "arrival": 0, "burst": 6, "priority": 2, "type": "CPU"},
    {"id": "P2", "arrival": 1, "burst": 4, "priority": 1, "type": "IO"}
  ]
}
```

## Algorithms Implemented

| Algorithm | Preemptive | Notes |
|---|---|---|
| FCFS | No | First-come first-served |
| SJF | No | Shortest job first |
| SRTF | Yes | Shortest remaining time first |
| Priority | No | Non-preemptive with starvation detection + aging |
| Priority (Preemptive) | Yes | Preemptive with starvation detection + aging |
| Round Robin | Yes | Dynamic quantum adjustment |
| **Adaptive** | — | Auto-selects based on workload |

## Adaptive Decision Engine

The engine uses **five workload characteristics** as inputs:

| Input | Description |
|---|---|
| `avg_burst` | Average burst time across all processes |
| `burst_variance` | Variance of burst times |
| `waiting_time_threshold` | Effective starvation threshold (adjusts with load) |
| `system_load` | Processes per time unit |
| `context_switch_rate` | Estimated context switch frequency (if RR chosen) |

### Decision Rules

```
variance < 4              → FCFS        (uniform workload)
priority_spread >= 4      → Priority    (critical process precedence)
variance < 20 & load<0.6  → SJF         (moderate variance, low load)
variance >= 20 & avg>8    → SRTF        (high variance, long bursts)
mixed / high load         → Round Robin (dynamic quantum, fairness)
```

## Dynamic Time Quantum (Round Robin)

When `--quantum 0` (default), the quantum is computed automatically:

```
base     = ceil(avg_burst × 0.4)
var_adj  = ceil(sqrt(variance) × 0.2)   # high variance → larger quantum
load_adj = n > 8 ? 1 : 0               # heavy load → slightly larger quantum
quantum  = clamp(base + var_adj + load_adj, 2, 20)
```

This reduces excessive context switching while maintaining responsiveness.

## Starvation Detection & Aging Mechanism

- Starvation detection triggers when a process has waited ≥ `starv-threshold` time units
- Each threshold cycle boosts effective priority by `aging-boost` (lower value = higher priority)
- Priority boost prevents indefinite postponement of low-priority processes
- All aging events are logged with timestamp, old/new priority, and accumulated wait time

## Performance Metrics

| Metric | Description |
|---|---|
| Waiting time | Time spent in ready queue |
| Turnaround time | Total time from arrival to completion |
| Response time | Time from arrival to first CPU allocation |
| Throughput | Processes completed per time unit |
| CPU utilization | % of time CPU is busy |
| Context switches | Total preemptions/dispatches |
| Context switch rate | Context switches per time unit |
| System load | Processes per time unit |
| Idle time | Total CPU idle time |
| **Jain's Fairness Index** | (Σwi)² / (n × Σwi²) — 1.0 = perfect fairness |

## Dependencies

- **C++17** — `g++ -std=c++17` (no external libraries)
- **Python 3.8+** — for Flask GUI bridge
- **Flask** — `pip install flask`
- Browser (Chrome/Firefox) for the GUI

## Requirements

```bash
sudo apt install g++ make python3 python3-pip   # Ubuntu/Debian
```
