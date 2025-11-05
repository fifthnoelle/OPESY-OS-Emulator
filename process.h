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

struct CustomProcessLines {
    vector<string> lines = {"DECLARE:       uint16_t var1 = 0", "DECLARE:       uint16_t var2 = 0", "DECLARE:       uint16_t var3 = 0"};         // all code lines (DECLARE, ADD, etc.)
    vector<string> runningLines = {"DECLARE:       uint16_t var1 = 0", "DECLARE:       uint16_t var2 = 0", "DECLARE:       uint16_t var3 = 0"};  // lines currently executing
    int lineNumber = 0;
};

struct ProcessStub {
    string name;
    int id;
    bool finished{false};
    bool attached{false};
    struct LogEntry { string timestamp; string message; };
    vector<LogEntry> logs;
    map<string, uint16_t> vars;
    CustomProcessLines code;
    mutex mtx;
};

/**My idea here is that 
 * 1. When user DECLAREs a variable, it gets added to vector lines as a whole string before return statement
 * 2. When user runs PRINT, DECLARE, ADD, SUB; vector runningLines + lines will add the corresponding code to itself
 * 3. I'm thinking that for ADD or SUB, if user adds or subtracts a variable, checks if variable exists in declared variables first
 * 4. I don't know how to deal with FOR yet
*/

/**
//I hope I understood the assignment ToT
struct CustomProcessLines{
    ProcessStub process;
    vector<string> lines = {"Declare:       unint16_t var1 = 0;","Declare:      uint16_t var2 = 0;","Declare:       uint16_t var3 = 0;", "Add/Subtract:     return var1;"}; //Idea here is that whenever a new declaration happens, they get added here as the whole string
    vector<string> runningLines = {"Declare:       unint16_t var1 = 0;","Declare:       uint16_t var2 = 0;","Declare:       uint16_t var3 = 0;"};
    vector<int> runningLineNumbers = {1,2,3,4};
    vector<uint16_t> uintVars = {0,0,0};
    vector<int> intVars = {0,0,0};
    vector<double> doubleVars = {0.0,0.0,0.0};
    vector<float> floatVars = {0.0f,0.0f,0.0f};
    vector<long> longVars = {0,0,0};
    vector<string> stringVars = {"Hello World from ...","Hello World from ...","Hello World from ..."};
    vector<char> charVars = {'a','b','c'};
    vector<bool> boolVars = {true,false,true};
    vector<string> checker = {"uint16_t","int","double","float","long","string","char","bool"}; //To check if variable is of available datatype
    
    //Please help how do you store into a loop a set of lines?
   
    int pause; //pause = sleep time and also sleep emulator for 5 ms passing per instruction if used in for loop
    int lineNumber = 0;
};
*/

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
inline void add_log(const shared_ptr<ProcessStub> &p, const string &msg, int core_id = -1) {
    if (!p) return;
    lock_guard<mutex> lk(p->mtx);
    ProcessStub::LogEntry e;
    e.timestamp = timestamp_now();

    // Only prefix with "Core <id>" if the message itself doesn't already mention it
    if (core_id >= 0 && msg.find("Core") == string::npos)
        e.message = "Core " + to_string(core_id) + ": " + msg;
    else
        e.message = msg;

    p->logs.push_back(move(e));
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



void generate_dummy_instructions(shared_ptr<ProcessStub> p, int num_instructions) {
    static const vector<string> ops = {"DECLARE", "ADD", "SUBTRACT", "PRINT", "SLEEP", "FOR"};
    for (int i = 0; i < num_instructions; ++i) {
        string op = ops[rand() % ops.size()];
        if (op == "DECLARE") {
            string var = "x" + to_string(i);
            int val = rand() % 100;
            p->code.lines.push_back("DECLARE " + var + " " + to_string(val));
        } else if (op == "ADD") {
            p->code.lines.push_back("ADD x0 x1 " + to_string(rand() % 10));
        } else if (op == "SUBTRACT") {
            p->code.lines.push_back("SUBTRACT x0 x1 " + to_string(rand() % 10));
        } else if (op == "PRINT") {
            p->code.lines.push_back("PRINT \"Hello world from " + p->name + "!\"");
        } else if (op == "SLEEP") {
            p->code.lines.push_back("SLEEP " + to_string(rand() % 200));
        } else if (op == "FOR") {
            int repeats = 1 + rand() % 3;
            for (int j = 0; j < repeats; ++j) {
                p->code.lines.push_back("PRINT \"FOR iteration " + to_string(j+1) + "\"");
            }
        }
    }
}

#endif