#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "config.h"
#include "process.h"
#include <thread>
#include <queue>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <iostream>

using namespace std;
extern std::atomic<int> active_cores;

class Scheduler {
public:
    Scheduler(const Config& cfg)
        : config(cfg), running(false) {}

    void start();
    void stop();
    bool is_running() const { return running.load(); }

    void enqueue(shared_ptr<ProcessStub> p) {
        lock_guard<mutex> lk(mtx);
        ready_queue.push(p);
        cv.notify_one();
    }

private:
    Config config;
    atomic<bool> running;
    vector<thread> cpu_threads;
    mutex mtx;
    condition_variable cv;

    queue<shared_ptr<ProcessStub>> ready_queue;

    void cpu_worker(int core_id);
    void execute_instruction(shared_ptr<ProcessStub> p);
};

void Scheduler::start() {
    if (running.load()) return;
    running.store(true);

    cout << "Scheduler started (" << config.scheduler << ")\n";

    for (int i = 0; i < config.num_cpu; ++i) {
        cpu_threads.emplace_back(&Scheduler::cpu_worker, this, i);
    }
}

void Scheduler::stop() {
    running.store(false);
    cv.notify_all();

    for (auto& t : cpu_threads) {
        if (t.joinable()) t.join();
    }
    cpu_threads.clear();

    cout << "Scheduler stopped.\n";
}

void Scheduler::cpu_worker(int core_id) {
    while (running.load()) {
        shared_ptr<ProcessStub> p = nullptr;

        {
            unique_lock<mutex> lk(mtx);
            cv.wait_for(lk, chrono::milliseconds(100), [this]() { return !ready_queue.empty() || !running.load(); });
            if (!running.load()) break;

            if (!ready_queue.empty()) {
                p = ready_queue.front();
                ready_queue.pop();
            }
        }

        if (p) execute_instruction(p);
    }
}

void Scheduler::execute_instruction(shared_ptr<ProcessStub> p) {
    if (!p) return;

    //atomic<int> active_cores;
    //active_cores++;

    active_cores.fetch_add(1, std::memory_order_relaxed);

    for (uint32_t i = 0; i < config.quantum_cycles && running.load(); ++i) {
        string instruction;
        {
            lock_guard<mutex> lk(p->mtx);
            if (p->code.lineNumber >= p->code.lines.size()) {
                p->finished = true;
                add_log(p, "Process finished execution.");
                break;
            }
            instruction = p->code.lines[p->code.lineNumber++];
        }

        add_log(p, "Executing: " + instruction);
        this_thread::sleep_for(chrono::milliseconds(config.delay_per_exec));

        if (instruction.find("SLEEP") != string::npos) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

    if (!p->finished && config.scheduler == "rr") {
        lock_guard<mutex> lk(mtx);
        ready_queue.push(p);
        cv.notify_one();
    }

    active_cores.fetch_sub(1, std::memory_order_relaxed);
}

#endif