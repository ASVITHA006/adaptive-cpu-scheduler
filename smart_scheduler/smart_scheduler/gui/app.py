import subprocess, json, os, sys
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

BASE_DIR    = os.path.dirname(os.path.abspath(__file__))
BINARY      = os.path.join(BASE_DIR, "..", "build", "scheduler")
CONFIG_PATH = os.path.join(BASE_DIR, "..", "config", "processes.json")

# ─── Helpers ──────────────────────────────────────────────────────────────────

def write_config(processes: list, starv_threshold: int, aging_boost: int):
    converted = []
    for p in processes:
        converted.append({
            "id":       p.get("pid", p.get("id", "P?")),
            "arrival":  p.get("arrival_time", p.get("arrival", 0)),
            "burst":    p.get("burst_time", p.get("burst", 1)),
            "priority": p.get("priority", 1)
        })
    config = {
        "starvation_threshold": starv_threshold,
        "aging_boost": aging_boost,
        "processes": converted
    }
    with open(CONFIG_PATH, "w") as f:
        json.dump(config, f, indent=2)

def normalize_result(result: dict) -> dict:
    """Normalize binary output field names to frontend field names."""
    for p in result.get("processes", []):
        if "id" in p and "pid" not in p:
            p["pid"] = p.pop("id")
        if "arrival" in p and "arrival_time" not in p:
            p["arrival_time"] = p.pop("arrival")
        if "burst" in p and "burst_time" not in p:
            p["burst_time"] = p.pop("burst")
        if "completion" in p and "completion_time" not in p:
            p["completion_time"] = p.pop("completion")
        if "turnaround" in p and "turnaround_time" not in p:
            p["turnaround_time"] = p.pop("turnaround")
        if "waiting" in p and "waiting_time" not in p:
            p["waiting_time"] = p.pop("waiting")
        if "response" in p and "response_time" not in p:
            p["response_time"] = p.pop("response")

    if "timeline" in result and "gantt" not in result:
        result["gantt"] = result.pop("timeline")

    return result

def run_scheduler(algo: str, quantum: int, starv_threshold: int, aging_boost: int) -> dict:
    if not os.path.exists(BINARY):
        raise RuntimeError(f"Scheduler binary not found at {BINARY}. Run 'make' first.")

    cmd = [BINARY,
           "--config", CONFIG_PATH,
           "--algo",   algo,
           "--starv-threshold", str(starv_threshold),
           "--aging-boost",     str(aging_boost)]
    if quantum > 0:
        cmd += ["--quantum", str(quantum)]

    result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "Scheduler exited with error.")

    data = json.loads(result.stdout)
    return normalize_result(data)

# ─── Routes ───────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/simulate", methods=["POST"])
def simulate():
    data = request.get_json()
    try:
        processes       = data.get("processes", [])
        algo            = data.get("algo", "adaptive")
        quantum         = int(data.get("quantum", -1))
        starv_threshold = int(data.get("starv_threshold", 15))
        aging_boost     = int(data.get("aging_boost", 1))

        if not processes:
            return jsonify({"error": "No processes provided"}), 400

        write_config(processes, starv_threshold, aging_boost)
        result = run_scheduler(algo, quantum, starv_threshold, aging_boost)
        return jsonify(result)

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/compare", methods=["POST"])
def compare():
    """Run all algorithms on the same process set and return metrics for each."""
    data      = request.get_json()
    processes = data.get("processes", [])
    starv     = int(data.get("starv_threshold", 15))
    boost     = int(data.get("aging_boost", 1))

    if not processes:
        return jsonify({"error": "No processes provided"}), 400

    write_config(processes, starv, boost)
    algos   = ["FCFS", "SJF", "SRTF", "Priority", "RR", "adaptive"]
    results = {}
    for algo in algos:
        try:
            r = run_scheduler(algo, -1, starv, boost)
            results[algo] = r.get("metrics", {})
            results[algo]["algorithm"] = r.get("algorithm", algo)
        except Exception as e:
            results[algo] = {"error": str(e)}

    return jsonify(results)

if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║   Smart Scheduler — CPU Scheduling GUI   ║")
    print("╚══════════════════════════════════════════╝")
    print(f"Binary : {BINARY}")
    print("Open   : http://localhost:5000\n")
    app.run(debug=True, port=5000)
