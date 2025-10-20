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
config.min_ins << std::endl;
config.max_ins << std::endl;
config.delay_per_exec << std::endl;
*/

//Scheduler integrated later in scheduler.h
//Scheduler scheduler(config);

//Flags for display
static std::atomic<bool> scheduler_running{false};
static std::thread scheduler_thread;
static std::condition_variable_any scheduler_cv;

//Util for clearing console
static void clear_console() {
    //Clear screen, implement later?
    for (int i = 0; i < 60; ++i) std::cout << '\n';
}

//Loop simulatin logging and finishing
static void scheduler_loop(int interval_ms) {
    while (scheduler_running.load()) {
        //Generate a dummy process name and create it
        std::string name;
        {
            std::lock_guard<std::mutex> lk(repository_mutex);
            int n = processes.size() + 1;
            std::ostringstream ss; ss << 'p' << std::setw(2) << std::setfill('0') << n;
            name = ss.str();
        }
        auto p = create_process(name);
        {
            std::lock_guard<std::mutex> lk(p->mtx);
            //p->logs.push_back("Hello world from " + p->name + "!");
        }

        //Let it run for a short time thwn mark finished later
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));

        //Randomly decide to finish some processes
        {
            if (!p->finished) {
                add_log(p, std::string("[finish] ") + p->name + " finished execution.");
                std::lock_guard<std::mutex> lk(p->mtx);
                p->finished = true;
            }
        }
    }
}

//Print summary works for displaying and writing to file
static void print_summary(std::ostream &out) {
    std::lock_guard<std::mutex> lk(repository_mutex);
    int total = processes.size();
    int running = 0, finished = 0;
    for (auto &kv : processes) {
        auto &p = kv.second;
        std::lock_guard<std::mutex> plk(p->mtx);
        if (p->finished) ++finished; else ++running;
    }

    out << "CPU Utilization (simulated): " << (running>0?50:0) << "%" << std::endl;
    out << "Cores used: " << running << std::endl;
    out << "Cores available: " << (4 - running) << " (simulated)\n" << std::endl;
    out << "---------------------------------------------------" << std::endl;
    out << "Running Processes:" << std::endl;
    for (auto &kv : processes) {
        auto &p = kv.second;
        std::lock_guard<std::mutex> plk(p->mtx);
        if (!p->finished) {
            std::string last_time = "-";
            if (!p->logs.empty()) last_time = p->logs.back().timestamp;
            out << p->name << " \t(" << last_time << ") \tCore: " << std::endl;
        }
    }

    out << "\nFinished Processes:" << std::endl;
    for (auto &kv : processes) {
        auto &p = kv.second;
        std::lock_guard<std::mutex> plk(p->mtx);
        if (p->finished) {
            std::string last_time = "-";
            if (!p->logs.empty()) last_time = p->logs.back().timestamp;
            out << p->name << " \t(" << last_time << ") \tFinished" << std::endl;
        }
    }
    out << "---------------------------------------------------" << std::endl;
}

//Save summary to file for report-util
static void save_report_util(const std::string &path) {
    std::ofstream ofs(path);
    if (!ofs) {
        std::cout << "Failed to open " << path << " for writing." << std::endl;
        return;
    }
    print_summary(ofs);
    ofs.close();
    std::cout << "Saved report to " << path << std::endl;
}

static void print_process(const std::shared_ptr<ProcessStub>& p) {
    std::cout << "\nProcess name: " << p->name << std::endl;
    std::cout << "ID: " << p->id << std::endl;
    std::cout << "Logs: " << std::endl;
    {
        std::lock_guard<std::mutex> plk(p->mtx);
        for (const auto &entry : p->logs) {
            std::cout << "(" << entry.timestamp << ")" << " Core: " << "core";
            std::cout << "\t\"" << entry.message << "\"" << std::endl;
        }
    }
    std::cout << "\nCurrent Instruction Line: " << std::endl;
    std::cout << "\nLines of Code: " << std::endl;
    std::cout << std::endl;
}

//Run process interactive screen
static void run_process_screen(const std::string& process_name) {
    std::shared_ptr<ProcessStub> p;
    {
        std::lock_guard<std::mutex> lk(repository_mutex);
        auto it = processes.find(process_name);
        if (it == processes.end()) {
            std::cout << "Process " << process_name << " not found." << std::endl;
            return;
        }
        p = it->second;
    }

    if (p->finished) {
        std::cout << "Process " << process_name << " has already finished." << std::endl;
        return;
    }

    clear_console();
    print_process(p);

    std::string line;
    while (true) {
        std::cout << "root:\\" << process_name << "\\> ";
        if (!std::getline(std::cin, line)) break;
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "exit") break;
        else if (cmd == "process-smi") {
            print_process(p);
        } else {
            std::cout << "Unknown command inside screen. Available: process-smi, exit" << std::endl;
        }
    }

    clear_console();
}

//Main menu loop
static void run_main_menu() {
    std::string command;

    std::cout << "Welcome to CSOPESY!" << std::endl;
    std::cout << "Version Date: October, 2025" << std::endl << std::endl;

    while (true) {
        std::cout << "root:\\> ";
        if (!std::getline(std::cin, command)) break;

        std::stringstream ss(command);
        std::string root;
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
                std::cout << "Failed to initialize: " << err.value() << std::endl;
            } else {
                initialized = true;
                std::cout << "Initialized from config.txt" << std::endl;
                std::cout << " num-cpu=" << config.num_cpu  << std::endl;
                std::cout << " scheduler=" << config.scheduler << std::endl;
                std::cout << " quantum-cycles=" << config.quantum_cycles << std::endl;
                std::cout << " batch-process-freq=" << config.batch_process_freq << std::endl;
                std::cout << " min-ins=" << config.min_ins << std::endl;
                std::cout << " max-ins=" << config.max_ins << std::endl;
                std::cout << " delay-per-exec=" << config.delay_per_exec << std::endl;
            }
            continue;
        }

        if (!initialized && root != "exit") {
            std::cout << "Error: Must run 'initialize' first." << std::endl;
            continue;
        }

        if (root == "screen") {
            std::string opt;
            ss >> opt;
            if (opt == "-s") {
                std::string pname;
                ss >> pname;
                if (pname.empty()) {
                    std::cout << "Usage: screen -s <process_name>" << std::endl;
                } else {
                    create_process(pname);
                    run_process_screen(pname);
                }
            } else if (opt == "-r") {
                std::string pname;
                ss >> pname;
                if (pname.empty()) {
                    std::cout << "Usage: screen -r <process_name>" << std::endl;
                } else {
                    run_process_screen(pname);
                }
            } else if (opt == "-ls") {
                print_summary(std::cout);
            } else {
                std::cout << "screen commands: -s <name> (create+attach), -r <name> (attach), -ls (list)" << std::endl;
            }
            continue;
        }

        if (root == "scheduler-start") {
            if (scheduler_running.load()) {
                std::cout << "Scheduler already running." << std::endl;
            } else {
                scheduler_running.store(true);
                // spawn thread for sim
                scheduler_thread = std::thread([](){ scheduler_loop(500); });
                std::cout << "Scheduler started (simulated)." << std::endl;
            }
            continue;
        }

        if (root == "scheduler-stop") {
            if (!scheduler_running.load()) {
                std::cout << "Scheduler is not running." << std::endl;
            } else {
                scheduler_running.store(false);
                if (scheduler_thread.joinable()) scheduler_thread.join();
                std::cout << "Scheduler stopped." << std::endl;
            }
            continue;
        }

        if (root == "report-util") {
            save_report_util("csopesy-log.txt");
            continue;
        }

        std::cout << "Unknown command. Available: initialize, exit, screen, scheduler-start, scheduler-stop, report-util" << std::endl;
    }
}

int main() {
    //May init dito for config and scheduler
    run_main_menu();
    return 0;
}
