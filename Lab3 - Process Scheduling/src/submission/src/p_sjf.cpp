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

using namespace std;

// A struct to log schedule entries in memory for later printing.
struct ScheduleEntry {
    int p_no;
    int burst_no;
    int start_time;
    int end_time;
};

// Custom comparator for the ready queue (min-heap on remaining_time).
struct CompareProcess {
    bool operator()(const Process* p1, const Process* p2) const {
        return p1->remaining_time > p2->remaining_time;
    }
};

void p_sjf(vector<Process>& processes, string file_name) {
    vector<ScheduleEntry> schedule_log;

    // --- Simulation Starts Here ---
    auto sim_start_timer = chrono::high_resolution_clock::now();

    priority_queue<Process*, vector<Process*>, CompareProcess> ready_queue;
    vector<Process*> io_wait_list;

    int simulation_time = 0;
    int completed_processes = 0;
    int next_arrival_idx = 0;
    Process* running_process = nullptr;
    int last_dispatch_time = 0;

    while (completed_processes < processes.size()) {
        // 1. Add newly arrived processes to the ready queue.
        while (next_arrival_idx < processes.size() && processes[next_arrival_idx].arrival <= simulation_time) {
            ready_queue.push(&processes[next_arrival_idx]);
            next_arrival_idx++;
        }

        // 2. Add processes finished with I/O to the ready queue.
        for (auto it = io_wait_list.begin(); it != io_wait_list.end(); ) {
            if ((*it)->start_when <= simulation_time) {
                ready_queue.push(*it);
                it = io_wait_list.erase(it);
            } else {
                ++it;
            }
        }

        // 3. Scheduling Decision: Check for preemption.
        if (running_process != nullptr && !ready_queue.empty()) {
            if (ready_queue.top()->remaining_time < running_process->remaining_time) {
                // Preempt! Log the slice of work that was just done.
                if (simulation_time > last_dispatch_time) {
                    schedule_log.push_back({running_process->p_no, (running_process->current - 1) / 2, last_dispatch_time, simulation_time - 1});
                }
                ready_queue.push(running_process);
                running_process = nullptr;
            }
        }
        
        // If CPU is idle, dispatch the shortest job.
        if (running_process == nullptr && !ready_queue.empty()) {
            running_process = ready_queue.top();
            ready_queue.pop();
            // If it's a new burst, set its remaining time from the main bursts list.
            if (running_process->remaining_time == 0) {
                 running_process->remaining_time = running_process->bursts[running_process->current];
            }
            last_dispatch_time = simulation_time;
        }

        // 4. Advance time. If idle, jump to the next event.
        if (running_process == nullptr) {
            int next_event_time = INT_MAX;
            if(next_arrival_idx < processes.size()) next_event_time = min(next_event_time, processes[next_arrival_idx].arrival);
            for(const auto& p_io : io_wait_list) next_event_time = min(next_event_time, p_io->start_when);
            
            simulation_time = (next_event_time == INT_MAX) ? simulation_time + 1 : next_event_time;
            continue;
        }
        
        simulation_time++;

        // 5. Update running process and handle burst completion.
        if (running_process != nullptr) {
            running_process->remaining_time--;
            if (running_process->remaining_time == 0) {
                // Log the final slice of work for this burst.
                schedule_log.push_back({running_process->p_no, (running_process->current - 1) / 2, last_dispatch_time, simulation_time - 1});
                
                running_process->current++; // Move to I/O burst index
                
                if (running_process->current < running_process->bursts.size()) {
                    running_process->start_when = simulation_time + running_process->bursts[running_process->current];
                    io_wait_list.push_back(running_process);
                    running_process->current++; // Move to next CPU burst index
                } else {
                    running_process->completed = simulation_time;
                    completed_processes++;
                }
                running_process = nullptr; // CPU is now idle
            }
        }
    }

    auto sim_end_timer = chrono::high_resolution_clock::now();
    // --- Simulation Ends Here ---

    // --- All File I/O and Printing Happens Below ---
    chrono::duration<double, milli> elapsed_ms = sim_end_timer - sim_start_timer;

    cout << file_name << endl;
    
    ofstream schedule_file(file_name);
    if (!schedule_file.is_open()) {
        cerr << "Error: Could not open schedule.txt for writing." << endl;
        return;
    }

    // 1. Write the schedule from the log.
    schedule_file << "CPU1\n";
    for (const auto& entry : schedule_log) {
        schedule_file << "P" << entry.p_no << "," << entry.burst_no + 1
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