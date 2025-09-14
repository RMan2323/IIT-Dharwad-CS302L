// p_sjf.cpp

#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <algorithm> // For std::min
#include <limits>    // For std::numeric_limits

#include "algos.h"

using namespace std;

// Custom comparator for the ready queue (min-heap on remaining_time).
struct CompareProcess {
    bool operator()(const Process* p1, const Process* p2) const {
        return p1->remaining_time > p2->remaining_time;
    }
};

void p_sjf(vector<Process>& processes) {
    ofstream schedule_file("schedule.txt");
    schedule_file << "CPU0" << endl;

    priority_queue<Process*, vector<Process*>, CompareProcess> ready_queue;
    vector<Process*> io_wait_list;

    int simulation_time = 0;
    int completed_processes = 0;
    int next_arrival_idx = 0;
    Process* running_process = nullptr;
    int last_dispatch_time = 0;

    cout << "\n--- Preemptive SJF 1-CPU Simulation Starting ---" << endl;

    while (completed_processes < processes.size()) {
        // 1. Add newly arrived processes to the ready queue.
        while (next_arrival_idx < processes.size() && processes[next_arrival_idx].arrival <= simulation_time) {
            cout << "Time " << simulation_time << ": Process " << processes[next_arrival_idx].p_no << " arrived." << endl;
            ready_queue.push(&processes[next_arrival_idx]);
            next_arrival_idx++;
        }

        // 2. Add processes finished with I/O to the ready queue.
        for (auto it = io_wait_list.begin(); it != io_wait_list.end(); ) {
            if ((*it)->start_when <= simulation_time) {
                cout << "Time " << simulation_time << ": Process " << (*it)->p_no << " finished I/O." << endl;
                ready_queue.push(*it);
                it = io_wait_list.erase(it);
            } else {
                ++it;
            }
        }

        // 3. Scheduling Decision
        // Check if preemption is needed.
        if (running_process != nullptr && !ready_queue.empty()) {
            if (ready_queue.top()->remaining_time < running_process->remaining_time) {
                // Preempt!
                cout << "Time " << simulation_time << ": Process " << ready_queue.top()->p_no << " (rem=" << ready_queue.top()->remaining_time
                     << ") preempts Process " << running_process->p_no << " (rem=" << running_process->remaining_time << ")." << endl;
                schedule_file << "P" << running_process->p_no << "," << (running_process->current - 1) / 2 + 1
                              << "\t" << last_dispatch_time << "\t" << simulation_time - 1 << endl;
                ready_queue.push(running_process);
                running_process = nullptr;
            }
        }
        
        // If CPU is idle, dispatch the shortest job.
        if (running_process == nullptr && !ready_queue.empty()) {
            running_process = ready_queue.top();
            ready_queue.pop();
            // If it's a new burst, set its remaining time.
            if (running_process->remaining_time == 0) {
                 running_process->remaining_time = running_process->bursts[running_process->current];
            }
            last_dispatch_time = simulation_time;
            cout << "Time " << simulation_time << ": Process " << running_process->p_no << " starts running." << endl;
        }

        // 4. Advance time to the next interesting event.
        if (running_process == nullptr && ready_queue.empty() && next_arrival_idx < processes.size()) {
            simulation_time = processes[next_arrival_idx].arrival;
            continue;
        }

        simulation_time++;

        // 5. Update running process and handle burst completion.
        if (running_process != nullptr) {
            running_process->remaining_time--;
            if (running_process->remaining_time == 0) {
                cout << "Time " << simulation_time << ": Process " << running_process->p_no << " finished CPU burst." << endl;
                schedule_file << "P" << running_process->p_no << "," << (running_process->current - 1) / 2 + 1
                              << "\t" << last_dispatch_time << "\t" << simulation_time - 1 << endl;
                
                running_process->current++; // Move to I/O burst
                
                if (running_process->current < running_process->bursts.size()) {
                    running_process->start_when = simulation_time + running_process->bursts[running_process->current];
                    io_wait_list.push_back(running_process);
                    running_process->current++; // Move to next CPU burst
                } else {
                    running_process->completed = simulation_time;
                    completed_processes++;
                }
                running_process = nullptr; // CPU is now idle
            }
        }
    }
    schedule_file.close();
    cout << "--- Preemptive SJF 1-CPU Simulation Finished ---" << endl;
}