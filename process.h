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
#include <algorithm>
#include <functional>


using namespace std;

struct ProcessStub {
    string name;
    int id;
    bool finished{false};
    bool attached{false};
    struct LogEntry { string timestamp; string message; };
    vector<LogEntry> logs;
    map<string, uint16_t> vars;
    mutex mtx;
};

inline map< string, shared_ptr<ProcessStub>> processes;
inline atomic<int> process_counter{0};
inline mutex repository_mutex;

// Timestamp
inline string timestamp_now() {
    using namespace chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    time_t t = system_clock::to_time_t(now);
    tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
     ostringstream oss;
    oss <<  put_time(&tm, "%Y/%m/%d %H:%M:%S");
    oss << '.' <<  setw(3) <<  setfill('0') << ms.count();
    return oss.str();
}

// Adds a log with a timestamp to the process (thread-safe)
inline void add_log(const  shared_ptr<ProcessStub> &p, const  string &msg) {
    if (!p) return;
     lock_guard< mutex> lk(p->mtx);
    ProcessStub::LogEntry e;
    e.timestamp = timestamp_now();
    e.message = msg;
    p->logs.push_back( move(e));
}

//Create a process if it doesn't exist
inline  shared_ptr<ProcessStub> create_process(const  string &name) {
     lock_guard< mutex> lk(repository_mutex);
    auto it = processes.find(name);
    if (it != processes.end()) return it->second;

    int id = ++process_counter;
    auto p =  make_shared<ProcessStub>();
    p->name = name;
    p->id = id;
    p->finished = false;
    p->attached = false;
    add_log(p,  string("Hello world from ") + p->name + "!");
    processes[name] = p;
    return p;
}

//Name gen
inline  string gen_auto_name() {
    int n = ++process_counter;
     ostringstream ss;
    ss << 'p' <<  setw(2) <<  setfill('0') << n;
    return ss.str();
}

inline auto arithmetic(vector<double> nums, string operation){

    uint16_t base1 = 5; 
    uint16_t base2 = 5;

    double result = 0;

    if(operation == "add"){

        for(double n : nums){
            result += n;
        }
    }
    else if(operation == "sub"){
        //result = nums[0];
        //sort(nums.begin(), nums.end(), greater<double>());
        for(double n: nums){
            //if(n == nums[0]) continue;
            result -= n;
        }
    }
    
    return result;

}

#endif