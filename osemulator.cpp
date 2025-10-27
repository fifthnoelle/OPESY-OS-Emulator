//g++ -std=c++17 -O2 -pthread -o osemulator.exe osemulator.cpp
//.\osemulator.exe
#include <iostream>
#include <sstream>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <vector>
#include <fstream>
#include <chrono>
#include <iomanip>
#include "config.h"
#include "process.h"
//#include "scheduler.h"

using namespace std;

//ProcessStub and repository helpers are provided in process.h

//Config (from config.txt after initialization)
static Config config;
static bool initialized = false;

/*
Use these to pass config values for scheduler
config.num_cpu = num_cpu
config.scheduler = scheduler
config.quantum_cycles = quantum_cycles
config.batch_process_freq = batch_process_freq
config.min_ins <<  endl;
config.max_ins <<  endl;
config.delay_per_exec <<  endl;
*/

//Scheduler into scheduler.h, also please look at scheduler_loop()
//Scheduler scheduler(config);

//Flags for display
static  atomic<bool> scheduler_running{false};
static  thread scheduler_thread;
static  condition_variable_any scheduler_cv;

//Util for clearing console
static void clear_console() {
    //Clear screen, implement later?
    for (int i = 0; i < 60; ++i)  cout << '\n';
}

//This is not a real scheduler, just simulating process creation and finishing, pls delete later
static void scheduler_loop(int interval_ms) {
    while (scheduler_running.load()) {
        //Generate a dummy process name and create it
         string name;
        {
             lock_guard< mutex> lk(repository_mutex);
            int n = processes.size() + 1;
             ostringstream ss; ss << 'p' <<  setw(2) <<  setfill('0') << n;
            name = ss.str();
        }
        auto p = create_process(name);
        {
             lock_guard< mutex> lk(p->mtx);
            //p->logs.push_back("Hello world from " + p->name + "!");
        }

        //Let it run for a short time thwn mark finished later
         this_thread::sleep_for( chrono::milliseconds(interval_ms));

        //Randomly decide to finish some processes
        {
            if (!p->finished) {
                 lock_guard< mutex> lk(p->mtx);
                p->finished = true;
            }
        }
    }
}

//Print summary works for displaying and writing to file
static void print_summary( ostream &out) {
     lock_guard< mutex> lk(repository_mutex);
    int total = processes.size();
    int running = 0, finished = 0;
    for (auto &kv : processes) {
        auto &p = kv.second;
         lock_guard< mutex> plk(p->mtx);
        if (p->finished) ++finished; else ++running;
    }

    out << "CPU Utilization (simulated): " << (running>0?50:0) << "%" <<  endl;
    out << "Cores used: " << running <<  endl;
    out << "Cores available: " << (4 - running) << " (simulated)\n" <<  endl;
    out << "---------------------------------------------------" <<  endl;
    out << "Running Processes:" <<  endl;
    for (auto &kv : processes) {
        auto &p = kv.second;
         lock_guard< mutex> plk(p->mtx);
        if (!p->finished) {
             string last_time = "-";
            if (!p->logs.empty()) last_time = p->logs.back().timestamp;
            out << p->name << " \t(" << last_time << ") \tCore: " <<  endl;
        }
    }

    out << "\nFinished Processes:" <<  endl;
    for (auto &kv : processes) {
        auto &p = kv.second;
         lock_guard< mutex> plk(p->mtx);
        if (p->finished) {
             string last_time = "-";
            if (!p->logs.empty()) last_time = p->logs.back().timestamp;
            out << p->name << " \t(" << last_time << ") \tFinished" <<  endl;
        }
    }
    out << "---------------------------------------------------" <<  endl;
}

//Save summary to file for report-util
static void save_report_util(const  string &path) {
     ofstream ofs(path);
    if (!ofs) {
         cout << "Failed to open " << path << " for writing." <<  endl;
        return;
    }
    print_summary(ofs);
    ofs.close();
     cout << "Saved report to " << path <<  endl;
}

static void print_process(const  shared_ptr<ProcessStub>& p) {
     cout << "\nProcess name: " << p->name <<  endl;
     cout << "ID: " << p->id <<  endl;
     cout << "Logs: " <<  endl;
    {
         lock_guard< mutex> plk(p->mtx);
        for (const auto &entry : p->logs) {
             cout << "(" << entry.timestamp << ")" << " Core: " << "core";
             cout << "\t\"" << entry.message << "\"" <<  endl;
        }
    }
     cout << "\nCurrent Instruction Line: " <<  endl;
     cout << "\nLines of Code: " <<  endl;
     cout <<  endl;
}

//Run process interactive screen
static void run_process_screen(const  string& process_name) {
     shared_ptr<ProcessStub> p;
    {
         lock_guard< mutex> lk(repository_mutex);
        auto it = processes.find(process_name);
        if (it == processes.end()) {
             cout << "Process " << process_name << " not found." <<  endl;
            return;
        }
        p = it->second;
    }

    if (p->finished) {
         cout << "Process " << process_name << " has already finished." <<  endl;
        return;
    }

    clear_console();
    print_process(p);

     string line;
    while (true) {
         cout << "root:\\" << process_name << "\\> ";
        if (! getline( cin, line)) break;
        stringstream ss(line);
        string cmd;
        ss >> cmd;

        stringstream numbers;
        string toadd;

        vector<double> nums;
        if (cmd == "exit") break;
        else if (cmd == "process-smi") {
            print_process(p);
        } 
        else if(cmd == "add" || cmd == "sub"){

            string inputs;
            do{
                cout << "Enter at least 2 numbers to add seperated by space: " << endl;
                getline(cin, inputs);
            
                numbers << inputs;

                while(getline(numbers, toadd, ' ')){

                    nums.push_back(stoi(toadd));

                }
            }while(nums.size() < 2);

            auto result = arithmetic(nums, "add");
            cout << "Result: " << result << endl;
        }
        else if(cmd == "print"){

        }
        else if(cmd == "sleep"){
            
        }
        else if(cmd == "declare"){

        }
        else if(cmd == "for"){

        }
        else {
             cout << "Unknown command inside screen. Available: process-smi, exit, add, sub" <<  endl;
        }
    }

    clear_console();
}

//Main menu loop
static void run_main_menu() {
     string command;

     cout << "Welcome to CSOPESY!" <<  endl;
     cout << "Version Date: October, 2025" <<  endl <<  endl;

    while (true) {
         cout << "root:\\> ";
        if (! getline( cin, command)) break;

         stringstream ss(command);
         string root;
        ss >> root;
        if (root.empty()) continue;

        if (root == "exit") {
            //Stop scheduler if running
            if (scheduler_running.load()) {
                scheduler_running.store(false);
                if (scheduler_thread.joinable()) scheduler_thread.join();
            }
            break;
        }

        if (root == "initialize") {
            //Load config.txt
            auto err = load_config_from_file("config.txt", config);
            if (err.has_value()) {
                 cout << "Failed to initialize: " << err.value() <<  endl;
            } else {
                initialized = true;
                 cout << "Initialized from config.txt" <<  endl;
                 cout << " num-cpu=" << config.num_cpu  <<  endl;
                 cout << " scheduler=" << config.scheduler <<  endl;
                 cout << " quantum-cycles=" << config.quantum_cycles <<  endl;
                 cout << " batch-process-freq=" << config.batch_process_freq <<  endl;
                 cout << " min-ins=" << config.min_ins <<  endl;
                 cout << " max-ins=" << config.max_ins <<  endl;
                 cout << " delay-per-exec=" << config.delay_per_exec <<  endl;
            }
            continue;
        }

        if (!initialized && root != "exit") {
             cout << "Error: Must run 'initialize' first." <<  endl;
            continue;
        }

        if (root == "screen") {
             string opt;
            ss >> opt;
            if (opt == "-s") {
                 string pname;
                ss >> pname;
                if (pname.empty()) {
                     cout << "Usage: screen -s <process_name>" <<  endl;
                } else {
                    create_process(pname);
                    run_process_screen(pname);
                }
            } else if (opt == "-r") {
                 string pname;
                ss >> pname;
                if (pname.empty()) {
                     cout << "Usage: screen -r <process_name>" <<  endl;
                } else {
                    run_process_screen(pname);
                }
            } else if (opt == "-ls") {
                print_summary( cout);
            } else {
                 cout << "screen commands: -s <name> (create+attach), -r <name> (attach), -ls (list)" <<  endl;
            }
            continue;
        }

        if (root == "scheduler-start") {
            if (scheduler_running.load()) {
                 cout << "Scheduler already running." <<  endl;
            } else {
                scheduler_running.store(true);
                // spawn thread for sim
                scheduler_thread =  thread([](){ scheduler_loop(500); });
                 cout << "Scheduler started (simulated)." <<  endl;
            }
            continue;
        }

        if (root == "scheduler-stop") {
            if (!scheduler_running.load()) {
                 cout << "Scheduler is not running." <<  endl;
            } else {
                scheduler_running.store(false);
                if (scheduler_thread.joinable()) scheduler_thread.join();
                 cout << "Scheduler stopped." <<  endl;
            }
            continue;
        }

        if (root == "report-util") {
            save_report_util("csopesy-log.txt");
            continue;
        }

         cout << "Unknown command. Available: initialize, exit, screen, scheduler-start, scheduler-stop, report-util" <<  endl;
    }
}

int main() {
    //May init dito for config and scheduler
    run_main_menu();
    return 0;
}
