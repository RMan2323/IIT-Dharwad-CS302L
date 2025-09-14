// p_sjf_2.cpp

#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <algorithm>
#include <limits>

#include "algos.h"

// Re-using the same comparator from the 1-CPU version
struct CompareProcess2 {
    bool operator()(const Process* p1, const Process* p2) const {
        return p1->remaining_time > p2->remaining_time;
    }
};

void p_sjf_2(vector<Process>& processes) {
    ofstream schedule_file("schedule_2.txt"); // Use a different file to avoid overwrite
    // schedule_file << "CPU Schedules" << endl;

    const int NUM_CPUS = 2;
    priority_queue<Process*, vector<Process*>, CompareProcess2> ready_queue;
    vector<Process*> io_wait_list;

    vector<Process*> running_on_cpu(NUM_CPUS, nullptr);
    vector<int> last_dispatch_time(NUM_CPUS, 0);

    int simulation_time = 0;
    int completed_processes = 0;
    int next_arrival_idx = 0;

    cout << "\n--- Preemptive SJF 2-CPU Simulation Starting ---" << endl;

    while (completed_processes < processes.size()) {
        // 1. Add newly arrived and I/O-finished processes to ready queue
        while (next_arrival_idx < processes.size() && processes[next_arrival_idx].arrival <= simulation_time) {
            ready_queue.push(&processes[next_arrival_idx]);
            next_arrival_idx++;
        }
        for (auto it = io_wait_list.begin(); it != io_wait_list.end(); ) {
            if ((*it)->start_when <= simulation_time) {
                ready_queue.push(*it);
                it = io_wait_list.erase(it);
            } else {
                ++it;
            }
        }

        // 2. Scheduling Decision
        // Attempt preemption if a shorter job is available
        if (!ready_queue.empty()) {
            for (int i = 0; i < NUM_CPUS; ++i) {
                // Find the running process with the longest remaining time
                int longest_rem_cpu_idx = -1;
                int max_rem_time = -1;
                for (int j = 0; j < NUM_CPUS; ++j) {
                    if (running_on_cpu[j] != nullptr && running_on_cpu[j]->remaining_time > max_rem_time) {
                        max_rem_time = running_on_cpu[j]->remaining_time;
                        longest_rem_cpu_idx = j;
                    }
                }

                // If a ready process is shorter than the longest running one, preempt
                if (longest_rem_cpu_idx != -1 && ready_queue.top()->remaining_time < max_rem_time) {
                    Process* preempted_process = running_on_cpu[longest_rem_cpu_idx];
                    schedule_file << "CPU" << longest_rem_cpu_idx + 1 << "\n" << "P" << preempted_process->p_no << "," << (preempted_process->current - 1) / 2 + 1
                                  << "\t" << last_dispatch_time[longest_rem_cpu_idx] << "\t" << simulation_time - 1 << endl;
                    ready_queue.push(preempted_process);
                    running_on_cpu[longest_rem_cpu_idx] = nullptr;
                }
            }
        }

        // Dispatch to idle CPUs
        for (int i = 0; i < NUM_CPUS; ++i) {
            if (running_on_cpu[i] == nullptr && !ready_queue.empty()) {
                running_on_cpu[i] = ready_queue.top();
                ready_queue.pop();
                if (running_on_cpu[i]->remaining_time == 0) {
                    running_on_cpu[i]->remaining_time = running_on_cpu[i]->bursts[running_on_cpu[i]->current];
                }
                last_dispatch_time[i] = simulation_time;
            }
        }

        // 3. Advance time
        if (ready_queue.empty() && next_arrival_idx < processes.size()) {
            bool all_cpus_idle = true;
            for(int i=0; i<NUM_CPUS; ++i) if(running_on_cpu[i] != nullptr) all_cpus_idle = false;
            if(all_cpus_idle) {
                 simulation_time = processes[next_arrival_idx].arrival;
                 continue;
            }
        }
        simulation_time++;

        // 4. Update running processes
        for (int i = 0; i < NUM_CPUS; ++i) {
            if (running_on_cpu[i] != nullptr) {
                running_on_cpu[i]->remaining_time--;
                if (running_on_cpu[i]->remaining_time == 0) {
                    Process* p = running_on_cpu[i];
                    schedule_file << "CPU" << i + 1 << "\n" << "P" << p->p_no << "," << (p->current - 1) / 2 + 1
                                  << "\t" << last_dispatch_time[i] << "\t" << simulation_time - 1 << endl;
                    
                    p->current++;
                    if (p->current < p->bursts.size()) {
                        p->start_when = simulation_time + p->bursts[p->current];
                        io_wait_list.push_back(p);
                        p->current++;
                    } else {
                        p->completed = simulation_time;
                        completed_processes++;
                    }
                    running_on_cpu[i] = nullptr;
                }
            }
        }
    }
    schedule_file.close();
    cout << "--- Preemptive SJF 2-CPU Simulation Finished ---" << endl;
}