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
//#include "config.h"
//#include "scheduler.h"
//#include "process.h"

struct ProcessStub {
    std::string name;
    int id;
    bool finished{false};
    std::vector<std::string> logs;
    std::map<std::string, uint16_t> vars;
    std::mutex mtx;
};

//Central process repo
static std::map<std::string, std::shared_ptr<ProcessStub>> process_repository;
static std::atomic<int> process_counter{0};
static std::mutex repository_mutex;

//Scheduler/config hooks (placeholders for now)
//Config config;
//Scheduler scheduler(config);

//Flags for display shell
static std::atomic<bool> scheduler_running{false};
static std::thread scheduler_thread;
static std::condition_variable_any scheduler_cv;

//Util for clearing console
static void clear_console() {
    //Clear screen, implement later?
    for (int i = 0; i < 60; ++i) std::cout << '\n';
}

//Create a new process stub
static std::shared_ptr<ProcessStub> create_process(const std::string& name) {
    std::lock_guard<std::mutex> lk(repository_mutex);
    auto it = process_repository.find(name);
    if (it != process_repository.end()) return it->second;

    int id = ++process_counter;
    auto p = std::make_shared<ProcessStub>();
    p->name = name;
    p->id = id;
    p->finished = false;
    p->logs.push_back("[init] Process: " + name);
    process_repository[name] = p;
    return p;
}

//Name generator
static std::string gen_auto_name() {
    int n = ++process_counter;
    std::ostringstream ss;
    ss << 'p' << std::setw(2) << std::setfill('0') << n;
    return ss.str();
}

//Loop simulatin logging and finishing
static void scheduler_loop(int interval_ms) {
    while (scheduler_running.load()) {
        //Generate a dummy process name and create it
        std::string name;
        {
            std::lock_guard<std::mutex> lk(repository_mutex);
            int n = process_repository.size() + 1;
            std::ostringstream ss; ss << 'p' << std::setw(2) << std::setfill('0') << n;
            name = ss.str();
        }
        auto p = create_process(name);
        {
            std::lock_guard<std::mutex> lk(p->mtx);
            p->logs.push_back("Hello world from " + p->name + "!");
        }

        //Let it run for a short time thwn mark finished later
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));

        //Randomly decide to finish some processes
        {
            std::lock_guard<std::mutex> lk(p->mtx);
            if (!p->finished) {
                p->logs.push_back("[finish] " + p->name + " finished execution.");
                p->finished = true;
            }
        }
    }
}

//Print summary
static void print_summary(std::ostream &out) {
    std::lock_guard<std::mutex> lk(repository_mutex);
    int total = process_repository.size();
    int running = 0, finished = 0;
    for (auto &kv : process_repository) {
        auto &p = kv.second;
        std::lock_guard<std::mutex> plk(p->mtx);
        if (p->finished) ++finished; else ++running;
    }

    out << "CPU Utilization (simulated): " << (running>0?50:0) << "%" << std::endl;
    out << "Cores used: " << running << std::endl;
    out << "Cores free: " << (4 - running) << " (simulated)" << std::endl;
    out << "Total processes: " << total << std::endl;
    out << "Running: " << running << "  Finished: " << finished << std::endl;
    out << "Processes:" << std::endl;
    for (auto &kv : process_repository) {
        auto &p = kv.second;
        std::lock_guard<std::mutex> plk(p->mtx);
        out << " - " << p->name << " (id=" << p->id << ") " << (p->finished?"[finished]":"[running]") << std::endl;
    }
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

//Run process interactive screen
static void run_process_screen(const std::string& process_name) {
    std::shared_ptr<ProcessStub> p;
    {
        std::lock_guard<std::mutex> lk(repository_mutex);
        auto it = process_repository.find(process_name);
        if (it == process_repository.end()) {
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
    std::cout << "Process name: " << process_name << std::endl;

    std::string line;
    while (true) {
        std::cout << "root:\\" << process_name << "\\> ";
        if (!std::getline(std::cin, line)) break;
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "exit") break;
        else if (cmd == "process-smi") {
            std::lock_guard<std::mutex> plk(p->mtx);
            std::cout << "Process: " << p->name << " ID=" << p->id << std::endl;
            if (p->finished) std::cout << "Finished!" << std::endl;
            std::cout << "Logs:" << std::endl;
            for (auto &l : p->logs) std::cout << "  " << l << std::endl;
        } else {
            std::cout << "Unknown command inside screen. Available: process-smi, exit" << std::endl;
        }
    }

    clear_console();
}

//Main menu loop
static void run_main_menu() {
    bool initialized = false; //Placeholder until config is added
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
            //Placeholder: read config.txt later
            initialized = true;
            std::cout << "Initialized (config.txt) placeholder" << std::endl;
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
            //save_report_util("csopesy-log.txt");
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
