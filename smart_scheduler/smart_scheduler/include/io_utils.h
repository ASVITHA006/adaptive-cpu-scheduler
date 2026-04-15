#pragma once
#include "process.h"
#include "scheduler.h"
#include <string>

// Load processes from JSON config file
std::vector<Process> load_config(const std::string& path);

// Serialize simulation result to JSON string
std::string result_to_json(const SimulationResult& r, const AdaptiveDecision& dec);

// Parse algorithm string
Algorithm parse_algorithm(const std::string& s);
