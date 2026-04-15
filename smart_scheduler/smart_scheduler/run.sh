#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo "============================================"
echo "   Smart Scheduler — Setup & Launch"
echo "============================================"
echo ""

# 1. Build C++ binary
echo "[1/3] Building C++ scheduler binary..."
make --no-print-directory
echo "      ✓ Binary ready at build/scheduler"
echo ""

# 2. Install Python deps
echo "[2/3] Installing Python dependencies..."
cd gui
pip install -q -r requirements.txt --break-system-packages 2>/dev/null || pip install -q -r requirements.txt
echo "      ✓ Flask installed"
echo ""

# 3. Launch GUI
echo "[3/3] Starting Smart Scheduler GUI..."
echo "      Open http://localhost:5000 in your browser"
echo ""
python3 app.py
