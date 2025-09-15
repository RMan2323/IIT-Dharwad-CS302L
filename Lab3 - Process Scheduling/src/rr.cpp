#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <chrono>  // Required for timing
#include <iomanip> // Required for formatting output

#include "algos.h"

using namespace std;

// Struct to log schedule entries in memory for later printing.
struct ScheduleEntry {
    int p_no;
    int burst_no;
    int start_time;
    int end_time;
};

// Struct to hold a process and its remaining burst time in the ready queue.
struct RRItem {
    Process proc;
    int remaining;
};

void rr(const vector<Process>& processes, int quantum) {
    // Make a mutable copy to track completion times and other state changes.
    vector<Process> local_processes = processes;
    vector<ScheduleEntry> schedule_log;

    // --- Simulation Starts Here ---
    auto sim_start_timer = chrono::high_resolution_clock::now();

    queue<RRItem> ready;
    int time = local_processes.empty() ? 0 : local_processes[0].arrival;
    int not_arrived = 0;
    int done = 0;
    
    // Initial population of the ready queue
    while (not_arrived < local_processes.size() && local_processes[not_arrived].arrival <= time) {
        ready.push({local_processes[not_arrived], local_processes[not_arrived].bursts[1]});
        not_arrived++;
    }

    while (done != local_processes.size()) {
        // If ready queue is empty, jump time to the next arrival.
        if (ready.empty()) {
            if (not_arrived < local_processes.size()) {
                time = local_processes[not_arrived].arrival;
                // Add all processes that have arrived by this new time.
                while (not_arrived < local_processes.size() && local_processes[not_arrived].arrival <= time) {
                    ready.push({local_processes[not_arrived], local_processes[not_arrived].bursts[1]});
                    not_arrived++;
                }
            }
            continue;
        }

        RRItem item = ready.front();
        ready.pop();
        Process current = item.proc;

        int burst_no = (current.current - 1) / 2;
        int start_time = max(time, current.start_when);
        int run_time = min(quantum, item.remaining);

        // Log the schedule entry to our in-memory vector.
        schedule_log.push_back({current.p_no, burst_no, start_time, start_time + run_time - 1});

        time = start_time + run_time;
        item.remaining -= run_time;

        // Add any processes that arrived during the run_time before handling the current one.
        while (not_arrived < local_processes.size() && local_processes[not_arrived].arrival <= time) {
            ready.push({local_processes[not_arrived], local_processes[not_arrived].bursts[1]});
            not_arrived++;
        }

        if (item.remaining > 0) {
            // Burst not finished, put it back into the queue.
            item.proc = current;
            ready.push(item);
        } else {
            // Burst finished.
            current.current++;
            if (current.current < current.bursts.size()) {
                // If there's an I/O burst, set its completion time and re-queue for the next CPU burst.
                current.start_when = time + current.bursts[current.current];
                current.current++;
                ready.push({current, current.bursts[current.current]});
            } else {
                // Process is complete.
                local_processes[current.p_no - 1].completed = time;
                done++;
            }
        }
    }

    auto sim_end_timer = chrono::high_resolution_clock::now();
    // --- Simulation Ends Here ---

    // --- All File I/O and Printing Happens Below ---
    chrono::duration<double, milli> elapsed_ms = sim_end_timer - sim_start_timer;

    ofstream schedule_file("schedule.txt");
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
    for (const auto& p : local_processes) {
        int turnaround = p.completed - p.arrival;
        total_turnaround_time += turnaround;
        if (turnaround > max_turnaround_time) {
            max_turnaround_time = turnaround;
        }
    }

    schedule_file << "\n--- Performance Metrics ---\n";
    schedule_file << fixed << setprecision(3);
    schedule_file << "Simulator Runtime: " << elapsed_ms.count() << " ms\n";
    schedule_file << "Average Turnaround Time: " << total_turnaround_time / local_processes.size() << "\n";
    schedule_file << "Maximum Turnaround Time: " << max_turnaround_time << "\n";
    
    schedule_file.close();
}