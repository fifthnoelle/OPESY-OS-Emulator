#ifndef CONFIG_H
#define CONFIG_H
#include <string>
#include <cstdint>
#include <optional>

struct Config {
    int num_cpu = 1; //[1,128]
    std::string scheduler = "rr"; //"fcfs" or "rr"
    uint32_t quantum_cycles = 5; //[1, 2^32-1]
    uint32_t batch_process_freq = 1; //[1, 2^32-1]
    uint32_t min_ins = 1; //[1, 2^32-1]
    uint32_t max_ins = 1; //[1, 2^32-1]
    uint32_t delay_per_exec = 0; //[0, 2^32-1]
};

#include <optional>
#include <string>
#include <functional>
#include <fstream>
#include <sstream>
#include <iostream>

static inline bool clamp_int(int &v, int lo, int hi) {
    if (v < lo) { v = lo; return false; }
    if (v > hi) { v = hi; return false; }
    return true;
}

static inline std::optional<std::string> load_config_from_file(const std::string &path, Config &out) {
    std::ifstream ifs(path);
    if (!ifs) return std::optional<std::string>("file-not-found");
    std::string line;
    while (std::getline(ifs, line)) {
        // trim
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        std::string s = line.substr(start, end - start + 1);
        if (s.empty() || s[0] == '#') continue;
        std::istringstream ss(s);
        std::string key;
        if (!(ss >> key)) continue;
        std::string val;
        ss >> std::ws;
        if (ss.peek() == '"') { ss.get(); std::getline(ss, val, '"'); }
        else ss >> val;
        try {
            if (key == "num-cpu") {
                int v = std::stoi(val);
                if (v < 1) v = 1; if (v > 128) v = 128;
                out.num_cpu = v;
            } else if (key == "scheduler") {
                if (val == "fcfs" || val == "rr") out.scheduler = val;
                else return std::optional<std::string>("invalid-scheduler");
            } else if (key == "quantum-cycles") {
                uint32_t v = static_cast<uint32_t>(std::stoul(val));
                if (v < 1) v = 1; out.quantum_cycles = v;
            } else if (key == "batch-process-freq") {
                uint32_t v = static_cast<uint32_t>(std::stoul(val));
                if (v < 1) v = 1; out.batch_process_freq = v;
            } else if (key == "min-ins") {
                uint32_t v = static_cast<uint32_t>(std::stoul(val));
                if (v < 1) v = 1; out.min_ins = v;
            } else if (key == "max-ins") {
                uint32_t v = static_cast<uint32_t>(std::stoul(val));
                if (v < 1) v = 1; out.max_ins = v;
            } else if (key == "delay-per-exec" || key == "delays-per-exec") {
                uint32_t v = static_cast<uint32_t>(std::stoul(val));
                out.delay_per_exec = v;
            }
        } catch (...) {
            return std::optional<std::string>("parse-error");
        }
    }
    if (out.max_ins < out.min_ins) out.max_ins = out.min_ins;
    return std::nullopt;
}

#endif
