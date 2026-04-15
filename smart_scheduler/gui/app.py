"""
Smart Scheduler – Flask bridge
Calls the C++ binary and serves the web GUI.
"""
import subprocess, json, os, sys, socket
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

GUI_DIR     = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(GUI_DIR)
BINARY      = os.path.join(PROJECT_DIR, "build", "scheduler")
CONFIG      = os.path.join(PROJECT_DIR, "config", "processes.json")

def run_binary(args: list) -> dict:
    cmd = [BINARY] + args
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
        if result.returncode != 0:
            return {"error": result.stderr or "Binary returned non-zero exit code."}
        return json.loads(result.stdout)
    except FileNotFoundError:
        return {"error": f"Binary not found at {BINARY}. Run 'make' first."}
    except json.JSONDecodeError as e:
        return {"error": f"Invalid JSON from binary: {e}"}
    except subprocess.TimeoutExpired:
        return {"error": "Scheduler timed out."}

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/run", methods=["POST"])
def api_run():
    body      = request.get_json(force=True)
    algo      = body.get("algo", "adaptive")
    quantum   = body.get("quantum", 0)
    threshold = body.get("starv_threshold", 15)
    boost     = body.get("aging_boost", 1)
    generate  = body.get("generate", "")
    seed      = body.get("seed", 42)
    processes = body.get("processes", [])

    args = ["--algo", algo,
            "--quantum", str(quantum),
            "--starv-threshold", str(threshold),
            "--aging-boost", str(boost)]

    if generate:
        args += ["--generate", generate, "--seed", str(seed)]
    elif processes:
        import tempfile
        cfg = {"starvation_threshold": threshold, "aging_boost": boost, "processes": processes}
        tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False)
        json.dump(cfg, tmp); tmp.close()
        args += ["--config", tmp.name]
    else:
        args += ["--config", CONFIG]

    return jsonify(run_binary(args))

@app.route("/api/compare", methods=["POST"])
def api_compare():
    body      = request.get_json(force=True)
    threshold = body.get("starv_threshold", 15)
    boost     = body.get("aging_boost", 1)
    generate  = body.get("generate", "")
    seed      = body.get("seed", 42)
    processes = body.get("processes", [])

    args = ["--algo", "compare",
            "--starv-threshold", str(threshold),
            "--aging-boost", str(boost)]

    if generate:
        args += ["--generate", generate, "--seed", str(seed)]
    elif processes:
        import tempfile
        cfg = {"starvation_threshold": threshold, "aging_boost": boost, "processes": processes}
        tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False)
        json.dump(cfg, tmp); tmp.close()
        args += ["--config", tmp.name]
    else:
        args += ["--config", CONFIG]

    return jsonify(run_binary(args))

@app.route("/api/presets", methods=["GET"])
def api_presets():
    return jsonify(["random","heavy","light","bursty","uniform","edge"])

def find_free_port(start: int) -> int:
    for port in range(start, start + 20):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                s.bind(("", port))
            return port
        except OSError:
            continue
    return start  # fallback, will fail loudly

if __name__ == "__main__":
    preferred = int(os.environ.get("PORT", 5001))
    port = find_free_port(preferred)
    print(f"\n  Smart Scheduler GUI → http://localhost:{port}\n")
    app.run(debug=False, host="0.0.0.0", port=port)
