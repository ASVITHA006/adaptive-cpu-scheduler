CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
SRCS     = src/main.cpp src/scheduler.cpp src/io_utils.cpp src/process.cpp
TARGET   = build/scheduler

all: build_dir $(TARGET)

build_dir:
	mkdir -p build

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf build/

run: all
	./$(TARGET) --config config/processes.json --algo adaptive

run-rr: all
	./$(TARGET) --algo RR

run-priority: all
	./$(TARGET) --algo Priority --starv-threshold 10 --aging-boost 1

run-srtf: all
	./$(TARGET) --algo SRTF

test: all
	@echo "=== FCFS ===" && ./$(TARGET) --algo FCFS | python3 -m json.tool | head -20
	@echo "=== SJF ===" && ./$(TARGET) --algo SJF  | python3 -m json.tool | head -20
	@echo "=== Adaptive ===" && ./$(TARGET) --algo adaptive | python3 -m json.tool | head -40

.PHONY: all clean run run-rr run-priority run-srtf test build_dir
