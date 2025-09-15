#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <algorithm>
#include <limits>
#include <chrono>  // Required for timing
#include <iomanip> // Required for formatting output

#include "algos.h"
#include <climits>

// A struct to log schedule entries for multiple CPUs.
struct ScheduleEntry2 {
    int cpu_id;
    int p_no;
    int burst_no;
    int start_time;
    int end_time;
};

struct CompareProcess2 {
    bool operator()(const Process* p1, const Process* p2) const {
        return p1->remaining_time > p2->remaining_time;
    }
};

void p_sjf_2(vector<Process>& processes) {
    vector<ScheduleEntry2> schedule_log;

    // --- Simulation Starts Here ---
    auto sim_start_timer = chrono::high_resolution_clock::now();

    const int NUM_CPUS = 2;
    priority_queue<Process*, vector<Process*>, CompareProcess2> ready_queue;
    vector<Process*> io_wait_list;
    vector<Process*> running_on_cpu(NUM_CPUS, nullptr);
    vector<int> last_dispatch_time(NUM_CPUS, 0);

    int simulation_time = 0;
    int completed_processes = 0;
    int next_arrival_idx = 0;

    while (completed_processes < processes.size()) {
        // 1. Add newly arrived and I/O-finished processes to ready queue.
        while (next_arrival_idx < processes.size() && processes[next_arrival_idx].arrival <= simulation_time) {
            ready_queue.push(&processes[next_arrival_idx]);
            next_arrival_idx++;
        }
        for (auto it = io_wait_list.begin(); it != io_wait_list.end(); ) {
            if ((*it)->start_when <= simulation_time) {
                ready_queue.push(*it);
                it = io_wait_list.erase(it);
            } else { ++it; }
        }

        // 2. Scheduling Decision: Check for preemption.
        if (!ready_queue.empty()) {
            // Find the running process with the longest remaining time.
            int longest_rem_cpu_idx = -1;
            int max_rem_time = 0; // Must be > 0 to be preemptible
            for (int j = 0; j < NUM_CPUS; ++j) {
                if (running_on_cpu[j] != nullptr && running_on_cpu[j]->remaining_time > max_rem_time) {
                    max_rem_time = running_on_cpu[j]->remaining_time;
                    longest_rem_cpu_idx = j;
                }
            }

            // If a ready process is shorter than the longest running one, preempt.
            if (longest_rem_cpu_idx != -1 && ready_queue.top()->remaining_time < max_rem_time) {
                Process* preempted_process = running_on_cpu[longest_rem_cpu_idx];
                if (simulation_time > last_dispatch_time[longest_rem_cpu_idx]) {
                    schedule_log.push_back({longest_rem_cpu_idx + 1, preempted_process->p_no, (preempted_process->current - 1) / 2, last_dispatch_time[longest_rem_cpu_idx], simulation_time - 1});
                }
                ready_queue.push(preempted_process);
                running_on_cpu[longest_rem_cpu_idx] = nullptr;
            }
        }

        // Dispatch to idle CPUs.
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

        // 3. Advance time.
        bool all_cpus_idle = true;
        for(int i=0; i<NUM_CPUS; ++i) if(running_on_cpu[i] != nullptr) all_cpus_idle = false;
        
        if (all_cpus_idle && ready_queue.empty()) {
            int next_event_time = INT_MAX;
            if(next_arrival_idx < processes.size()) next_event_time = min(next_event_time, processes[next_arrival_idx].arrival);
            for(const auto& p_io : io_wait_list) next_event_time = min(next_event_time, p_io->start_when);

            if(next_event_time != INT_MAX) simulation_time = next_event_time;
            else break; // No more events
            continue;
        }
        simulation_time++;

        // 4. Update running processes.
        for (int i = 0; i < NUM_CPUS; ++i) {
            if (running_on_cpu[i] != nullptr) {
                running_on_cpu[i]->remaining_time--;
                if (running_on_cpu[i]->remaining_time == 0) {
                    Process* p = running_on_cpu[i];
                    schedule_log.push_back({i + 1, p->p_no, (p->current - 1) / 2, last_dispatch_time[i], simulation_time - 1});
                    
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

    auto sim_end_timer = chrono::high_resolution_clock::now();
    // --- Simulation Ends Here ---
    
    // --- All File I/O and Printing Happens Below ---
    chrono::duration<double, milli> elapsed_ms = sim_end_timer - sim_start_timer;

    ofstream schedule_file("schedule_2.txt");
    if (!schedule_file.is_open()) {
        cerr << "Error: Could not open schedule_2.txt for writing." << endl;
        return;
    }

    // 1. Write the schedule from the log.
    for (const auto& entry : schedule_log) {
        schedule_file << "CPU" << entry.cpu_id << "\n" << "P" << entry.p_no << "," << entry.burst_no
                      << "\t" << entry.start_time << "\t" << entry.end_time << "\n";
    }

    // 2. Calculate and write the final statistics.
    double total_turnaround_time = 0;
    int max_turnaround_time = 0;
    for (const auto& p : processes) {
        int turnaround = p.completed - p.arrival;
        total_turnaround_time += turnaround;
        if (turnaround > max_turnaround_time) {
            max_turnaround_time = turnaround;
        }
    }

    schedule_file << "\n--- Performance Metrics ---\n";
    schedule_file << fixed << setprecision(3);
    schedule_file << "Simulator Runtime: " << elapsed_ms.count() << " ms\n";
    schedule_file << "Average Turnaround Time: " << total_turnaround_time / processes.size() << "\n";
    schedule_file << "Maximum Turnaround Time: " << max_turnaround_time << "\n";

    schedule_file.close();
}