#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <iomanip>
#include <chrono> // Required for timing

#include "algos.h"

using namespace std;

// Structs are moved here for self-containment, but could be in a shared header
enum EventType { ARRIVAL, CPU_DONE, IO_DONE };

struct Event {
    int time;
    int process_id;
    EventType type;
    int cpu_id;

    bool operator>(const Event& other) const {
        if (time == other.time) return type > other.type;
        return time > other.time;
    }
};

struct ScheduleEntry2 {
    int cpu_id;
    int p_no;
    int burst_no;
    int start_time;
    int end_time;
};


void fifo_2(vector<Process>& processes, string file_name) {
    const int NUM_CPUS = 2;
    vector<ScheduleEntry2> schedule_log;

    // --- Simulation Starts Here ---
    auto sim_start_timer = chrono::high_resolution_clock::now();
    
    int simulation_time = 0;
    int completed_processes = 0;
    vector<int> cpu_free_time(NUM_CPUS, 0);
    priority_queue<Event, vector<Event>, greater<Event>> event_queue;
    queue<int> ready_queue;

    for (const auto& p : processes) {
        event_queue.push({p.arrival, p.p_no - 1, ARRIVAL, -1});
    }

    while (completed_processes < processes.size()) {
        if (event_queue.empty()) break;

        Event current_event = event_queue.top();
        event_queue.pop();
        simulation_time = current_event.time;
        Process& current_process = processes[current_event.process_id];

        switch (current_event.type) {
            case ARRIVAL:
                ready_queue.push(current_process.p_no - 1);
                break;
            case CPU_DONE:
                current_process.current++;
                if (current_process.current < current_process.bursts.size()) {
                    int io_duration = current_process.bursts[current_process.current];
                    event_queue.push({simulation_time + io_duration, current_process.p_no - 1, IO_DONE, -1});
                } else {
                    current_process.completed = simulation_time;
                    completed_processes++;
                }
                break;
            case IO_DONE:
                current_process.current++;
                ready_queue.push(current_process.p_no - 1);
                break;
        }

        for (int i = 0; i < NUM_CPUS; ++i) {
            if (cpu_free_time[i] <= simulation_time && !ready_queue.empty()) {
                int pid_to_run = ready_queue.front();
                ready_queue.pop();
                Process& p_to_run = processes[pid_to_run];

                int start_time = simulation_time;
                int cpu_duration = p_to_run.bursts[p_to_run.current];
                int finish_time = start_time + cpu_duration;

                cpu_free_time[i] = finish_time;
                event_queue.push({finish_time, p_to_run.p_no - 1, CPU_DONE, i});
                
                // Log the schedule entry instead of printing
                schedule_log.push_back({i + 1, p_to_run.p_no, (p_to_run.current - 1) / 2, start_time, finish_time});
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
        cerr << "Error: Could not open schedule_2.txt for writing." << endl;
        return;
    }

    // 1. Write the schedule from the log
    for (const auto& entry : schedule_log) {
        schedule_file << "CPU" << entry.cpu_id << "\n" << "P" << entry.p_no << "," << entry.burst_no + 1
                      << "\t" << entry.start_time << "\t" << entry.end_time << "\n";
    }

    // 2. Calculate and write the final statistics
    double total_turnaround_time = 0;
    int max_turnaround_time = 0;
    for (const auto& p : processes) {
        if(p.completed > 0) { // Ensure process was completed
            int turnaround = p.completed - p.arrival;
            total_turnaround_time += turnaround;
            if (turnaround > max_turnaround_time) {
                max_turnaround_time = turnaround;
            }
        }
    }
    
    schedule_file << "\n--- Performance Metrics ---\n";
    schedule_file << fixed << setprecision(3);
    schedule_file << "Simulator Runtime: " << elapsed_ms.count() << " ms\n";
    schedule_file << "Average Turnaround Time: " << total_turnaround_time / processes.size() << "\n";
    schedule_file << "Maximum Turnaround Time: " << max_turnaround_time << "\n";

    schedule_file.close();
}