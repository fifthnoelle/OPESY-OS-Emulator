#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <thread>
#include <queue>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include "process.h"
#include "config.h"

using namespace std;

// Global core utilization tracker
extern atomic<int> active_cores;

class Scheduler {
private:
    Config config;
    atomic<bool> running{false};
    vector<thread> core_threads;
    queue<shared_ptr<ProcessStub>> ready_queue;
    mutable mutex mtx;               // <-- mutable allows locking in const functions
    condition_variable cv;

    // Track per-core status
    vector<bool> core_active;
    vector<string> core_process;

public:
    Scheduler(const Config &cfg) 
        : config(cfg), 
          core_active(cfg.num_cpu, false),
          core_process(cfg.num_cpu, "") {}

    void add_process(shared_ptr<ProcessStub> p) {
        lock_guard<mutex> lk(mtx);
        ready_queue.push(p);
        cv.notify_one();
    }

    void start() {
        if (running.load()) return;
        running.store(true);

        cout << "Scheduler started (" << config.scheduler
             << ") with " << config.num_cpu << " cores." << endl;

        // launch threads first
        for (int i = 0; i < config.num_cpu; ++i)
            core_threads.emplace_back(&Scheduler::core_loop, this, i);

        // now create and enqueue processes
        int num_processes = 3;
        for (int i = 0; i < num_processes; ++i) {
            string pname = "p" + to_string(i + 1);
            auto p = create_process(pname);
            int num_ins = config.min_ins + (rand() % (config.max_ins - config.min_ins + 1));
            generate_dummy_instructions(p, num_ins);
            add_log(p, "Generated " + to_string(num_ins) + " randomized instructions");
            add_process(p);   // this will now properly wake waiting threads
        }
    }

    void stop() {
        running.store(false);
        cv.notify_all();
        for (auto &t : core_threads)
            if (t.joinable()) t.join();
        core_threads.clear();
        cout << "Scheduler stopped." << endl;
    }

    bool is_running() const { return running.load(); }

    // Accessors for reporting
    vector<bool> get_active_cores() const {
        lock_guard<mutex> lk(mtx);
        return core_active;
    }

    vector<string> get_core_processes() const {
        lock_guard<mutex> lk(mtx);
        return core_process;
    }

private:
    void core_loop(int core_id) {
        int tick = 0;
        while (running.load()) {
            shared_ptr<ProcessStub> p;

            {
                unique_lock<mutex> lk(mtx);
                cv.wait(lk, [&]() { return !ready_queue.empty() || !running.load(); });
                if (!running.load()) break;
                p = ready_queue.front();
                ready_queue.pop();

                // Mark this core as busy
                core_active[core_id] = true;
                core_process[core_id] = p->name;
                active_cores.fetch_add(1);
            }

            if (!p) continue;

            add_log(p, "Core " + to_string(core_id + 1) + ": Picked process " + p->name);

            if (config.scheduler == "fcfs") {
                add_log(p, "Core " + to_string(core_id) + ": Starting FCFS job", core_id + 1);
                int exec_time = rand() % 6 + 10;
                for (int i = 0; i < exec_time && running.load(); ++i) {
                    this_thread::sleep_for(chrono::milliseconds(config.delay_per_exec));
                    add_log(p, "Core " + to_string(core_id + 1) + ": Executing instruction " + to_string(i + 1), core_id + 1);
                }
                {
                    lock_guard<mutex> lk(p->mtx);
                    p->finished = true;
                    add_log(p, "Core " + to_string(core_id + 1) + ": FCFS job finished", core_id + 1);
                }
            } else if (config.scheduler == "rr") {
                int quantum = config.quantum_cycles;
                add_log(p, "Core " + to_string(core_id + 1) + ": Starting RR job", core_id + 1);

                for (int q = 0; q < quantum && running.load(); ++q) {
                    this_thread::sleep_for(chrono::milliseconds(config.delay_per_exec));
                    ++tick;
                    add_log(p, "Core " + to_string(core_id + 1) + ": RR tick " + to_string(tick), core_id + 1);
                }

                {
                    lock_guard<mutex> lk(mtx);
                    if (running.load()) {
                        add_log(p, "Core " + to_string(core_id + 1) + ": Time slice complete — requeuing");
                        ready_queue.push(p);
                        cv.notify_one();
                    } else {
                        p->finished = true;
                        add_log(p, "Core " + to_string(core_id + 1) + ": RR job finished", core_id + 1);
                    }
                }
            }

            // Mark this core as idle
            {
                lock_guard<mutex> lk(mtx);
                core_active[core_id] = false;
                core_process[core_id] = "";
                active_cores.fetch_sub(1);
            }
        }
    }
};

#endif
