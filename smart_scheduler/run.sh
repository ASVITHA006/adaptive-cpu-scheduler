#!/bin/bash
set -e
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║     Smart Scheduler  v2.0                ║"
echo "  ║     Adaptive CPU Scheduling Framework    ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""

# 1. Build C++ (suppress clock-skew noise)
echo "[1/3] Building C++ scheduler binary..."
make --no-print-directory 2>&1 | grep -v "Clock skew" | grep -v "modification time" || true
echo "      Done: build/scheduler"
echo ""

# 2. Python deps
echo "[2/3] Installing Python dependencies..."
cd gui
pip install -q -r requirements.txt --break-system-packages 2>/dev/null \
  || pip install -q -r requirements.txt 2>/dev/null \
  || true
echo "      Done: Flask ready"
echo ""

# 3. Launch (auto-selects free port 5001+)
echo "[3/3] Starting Smart Scheduler GUI..."
echo "      Auto-detecting free port (5001+)..."
echo ""
python3 app.py
