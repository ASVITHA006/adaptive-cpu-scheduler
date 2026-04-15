#include "../include/io_utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <cmath>

// ─── Minimal JSON helpers ─────────────────────────────────────────────────────

static std::string jstr(const std::string& s) { return "\"" + s + "\""; }
static std::string jnum(double v, int prec = 2) {
    std::ostringstream o; o << std::fixed << std::setprecision(prec) << v;
    return o.str();
}

// ─── Parse algorithm string ───────────────────────────────────────────────────

Algorithm parse_algorithm(const std::string& s) {
    if (s == "FCFS")      return Algorithm::FCFS;
    if (s == "SJF")       return Algorithm::SJF;
    if (s == "SRTF")      return Algorithm::SRTF;
    if (s == "Priority")  return Algorithm::PRIORITY;
    if (s == "PriorityP") return Algorithm::PRIORITY_PREEMPTIVE;
    if (s == "RR")        return Algorithm::ROUND_ROBIN;
    return Algorithm::ADAPTIVE;
}

// ─── JSON config loader ───────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\"");
    size_t b = s.find_last_not_of(" \t\r\n\"");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

static std::string extract_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        return json.substr(pos + 1, end - pos - 1);
    }
    size_t end = json.find_first_of(",}\n]", pos);
    return trim(json.substr(pos, end - pos));
}

std::vector<Process> load_config(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open config: " + path);
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    std::vector<Process> procs;
    size_t arr = json.find("\"processes\"");
    if (arr == std::string::npos) throw std::runtime_error("No 'processes' array in config.");
    arr = json.find('[', arr);
    size_t end_arr = json.find(']', arr);

    size_t pos = arr + 1;
    while (pos < end_arr) {
        size_t obj_start = json.find('{', pos);
        if (obj_start == std::string::npos || obj_start >= end_arr) break;
        size_t obj_end = json.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj = json.substr(obj_start, obj_end - obj_start + 1);
        Process p;
        p.id           = extract_value(obj, "id");
        p.arrival_time = std::stoi(extract_value(obj, "arrival").empty() ? "0" : extract_value(obj, "arrival"));
        p.burst_time   = std::stoi(extract_value(obj, "burst").empty()   ? "1" : extract_value(obj, "burst"));
        p.priority     = std::stoi(extract_value(obj, "priority").empty()? "3" : extract_value(obj, "priority"));
        std::string t  = extract_value(obj, "type");
        p.type         = (t == "IO") ? ProcessType::IO : (t == "Mixed") ? ProcessType::MIXED : ProcessType::CPU;
        p.remaining_time     = p.burst_time;
        p.effective_priority = p.priority;
        if (!p.id.empty()) procs.push_back(p);
        pos = obj_end + 1;
    }
    return procs;
}

// ─── Result → JSON ────────────────────────────────────────────────────────────

std::string result_to_json(const SimulationResult& r, const AdaptiveDecision& dec) {
    std::ostringstream o;
    o << "{\n";
    o << "  " << jstr("algorithm") << ": " << jstr(r.algorithm) << ",\n";
    o << "  " << jstr("quantum")   << ": " << r.quantum << ",\n";

    // Full adaptive decision block — includes all 5 workload inputs from abstract
    o << "  " << jstr("adaptive_decision") << ": {\n";
    o << "    " << jstr("reason")                << ": " << jstr(dec.reason) << ",\n";
    o << "    " << jstr("avg_burst")             << ": " << jnum(dec.avg_burst) << ",\n";
    o << "    " << jstr("burst_variance")        << ": " << jnum(dec.burst_variance) << ",\n";
    o << "    " << jstr("priority_spread")       << ": " << dec.priority_spread << ",\n";
    o << "    " << jstr("system_load")           << ": " << jnum(dec.system_load, 4) << ",\n";
    o << "    " << jstr("context_switch_rate")   << ": " << jnum(dec.context_switch_rate, 4) << ",\n";
    o << "    " << jstr("waiting_time_threshold")<< ": " << jnum(dec.waiting_time_threshold, 1) << ",\n";
    o << "    " << jstr("quantum")               << ": " << dec.quantum << "\n";
    o << "  },\n";

    // Timeline
    o << "  " << jstr("timeline") << ": [\n";
    for (size_t i = 0; i < r.timeline.size(); i++) {
        auto& s = r.timeline[i];
        o << "    {" << jstr("pid") << ": " << jstr(s.pid)
          << ", " << jstr("start") << ": " << s.start
          << ", " << jstr("end")   << ": " << s.end << "}";
        if (i + 1 < r.timeline.size()) o << ",";
        o << "\n";
    }
    o << "  ],\n";

    // Processes
    o << "  " << jstr("processes") << ": [\n";
    for (size_t i = 0; i < r.processes.size(); i++) {
        auto& p = r.processes[i];
        o << "    {";
        o << jstr("id")         << ": " << jstr(p.id) << ", ";
        o << jstr("arrival")    << ": " << p.arrival_time << ", ";
        o << jstr("burst")      << ": " << p.burst_time << ", ";
        o << jstr("priority")   << ": " << p.priority << ", ";
        o << jstr("completion") << ": " << p.completion_time << ", ";
        o << jstr("turnaround") << ": " << p.turnaround_time() << ", ";
        o << jstr("waiting")    << ": " << p.waiting_time() << ", ";
        o << jstr("response")   << ": " << p.response_time();
        o << "}";
        if (i + 1 < r.processes.size()) o << ",";
        o << "\n";
    }
    o << "  ],\n";

    // Aging events
    o << "  " << jstr("aging_events") << ": [\n";
    for (size_t i = 0; i < r.aging_events.size(); i++) {
        auto& e = r.aging_events[i];
        o << "    {";
        o << jstr("pid")          << ": " << jstr(e.pid) << ", ";
        o << jstr("time")         << ": " << e.time << ", ";
        o << jstr("old_priority") << ": " << e.old_priority << ", ";
        o << jstr("new_priority") << ": " << e.new_priority << ", ";
        o << jstr("wait_time")    << ": " << e.wait_time;
        o << "}";
        if (i + 1 < r.aging_events.size()) o << ",";
        o << "\n";
    }
    o << "  ],\n";

    // Metrics — includes all performance metrics from abstract
    o << "  " << jstr("metrics") << ": {\n";
    o << "    " << jstr("context_switches")     << ": " << r.context_switches << ",\n";
    o << "    " << jstr("context_switch_rate")  << ": " << jnum(r.context_switch_rate, 4) << ",\n";
    o << "    " << jstr("idle_time")            << ": " << r.idle_time << ",\n";
    o << "    " << jstr("total_time")           << ": " << r.total_time << ",\n";
    o << "    " << jstr("system_load")          << ": " << jnum(r.system_load, 4) << ",\n";
    o << "    " << jstr("avg_waiting_time")     << ": " << jnum(r.avg_waiting_time()) << ",\n";
    o << "    " << jstr("avg_turnaround_time")  << ": " << jnum(r.avg_turnaround_time()) << ",\n";
    o << "    " << jstr("avg_response_time")    << ": " << jnum(r.avg_response_time()) << ",\n";
    o << "    " << jstr("throughput")           << ": " << jnum(r.throughput(), 4) << ",\n";
    o << "    " << jstr("cpu_utilization")      << ": " << jnum(r.cpu_utilization()) << ",\n";
    o << "    " << jstr("jains_fairness_index") << ": " << jnum(r.jains_fairness_index(), 4) << "\n";
    o << "  }\n";
    o << "}\n";
    return o.str();
}
