#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>

struct ProcessStub {
    std::string name;
    int id;
    bool finished{false};
    bool attached{false};
    struct LogEntry { std::string timestamp; std::string message; };
    std::vector<LogEntry> logs;
    std::map<std::string, uint16_t> vars;
    std::mutex mtx;
};

inline std::map<std::string, std::shared_ptr<ProcessStub>> processes;
inline std::atomic<int> process_counter{0};
inline std::mutex repository_mutex;

// Timestamp
inline std::string timestamp_now() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");
    oss << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

// Adds a log with a timestamp to the process (thread-safe)
inline void add_log(const std::shared_ptr<ProcessStub> &p, const std::string &msg) {
    if (!p) return;
    std::lock_guard<std::mutex> lk(p->mtx);
    ProcessStub::LogEntry e;
    e.timestamp = timestamp_now();
    e.message = msg;
    p->logs.push_back(std::move(e));
}

//Create a process if it doesn't exist
inline std::shared_ptr<ProcessStub> create_process(const std::string &name) {
    std::lock_guard<std::mutex> lk(repository_mutex);
    auto it = processes.find(name);
    if (it != processes.end()) return it->second;

    int id = ++process_counter;
    auto p = std::make_shared<ProcessStub>();
    p->name = name;
    p->id = id;
    p->finished = false;
    p->attached = false;
    add_log(p, std::string("Hello world from ") + p->name + "!");
    processes[name] = p;
    return p;
}

//Name gen
inline std::string gen_auto_name() {
    int n = ++process_counter;
    std::ostringstream ss;
    ss << 'p' << std::setw(2) << std::setfill('0') << n;
    return ss.str();
}

#endif