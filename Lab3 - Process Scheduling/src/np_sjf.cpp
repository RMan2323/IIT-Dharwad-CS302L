#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <chrono>    // Required for timing
#include <iomanip>   // Required for formatting output

#include "algos.h"

using namespace std;

// Struct to log schedule entries in memory for later printing.
struct ScheduleEntry {
    int p_no;
    int burst_no;
    int start_time;
    int end_time;
};

class comp {
   public:
    bool operator()(const Process& p1, const Process& p2) {
        // SJF compares the duration of the current CPU burst
        return p1.bursts[p1.current] > p2.bursts[p2.current];
    }
};

void np_sjf(const vector<Process>& processes, string file_name) {
    // Make a mutable copy to track completion times and other state changes.
    vector<Process> local_processes = processes;
    vector<ScheduleEntry> schedule_log;

    // --- Simulation Starts Here ---
    auto sim_start_timer = chrono::high_resolution_clock::now();

    priority_queue<Process, vector<Process>, comp> ready;
    int time = local_processes.empty() ? 0 : local_processes[0].arrival;
    int not_arrived = 0;
    int done = 0;

    while (done != local_processes.size()) {
        // Add all processes that have arrived by the current time to the ready queue.
        for (; not_arrived < local_processes.size();) {
            if (local_processes[not_arrived].arrival <= time) {
                ready.push(local_processes[not_arrived]);
                not_arrived++;
            } else {
                break;
            }
        }

        // If the ready queue is empty, no process can run. Jump time to the next arrival.
        if (ready.empty()) {
            if (not_arrived < local_processes.size()) {
                time = local_processes[not_arrived].arrival;
            }
            continue; // Restart the loop to add the newly arrived process
        }

        Process current = ready.top();
        ready.pop();

        // If the process finished I/O after the current time, update time.
        if (current.start_when > time) {
            time = current.start_when;
        }

        int start_time = time;
        int duration = current.bursts[current.current];
        time += duration;

        // Log the schedule entry to our in-memory vector.
        schedule_log.push_back({current.p_no, (current.current - 1) / 2, start_time, time - 1});

        current.current++; // Move index past the CPU burst.

        // Check if the process has an I/O burst to follow.
        if (current.current < current.bursts.size()) {
            current.start_when = time + current.bursts[current.current]; // Set time it's ready after I/O
            current.current++; // Move index past the I/O burst.
            
            // Re-queue the process for its next CPU burst, unless it's done.
            if (current.current < current.bursts.size()) {
                 ready.push(current);
            } else { // Process ends after its final I/O.
                local_processes[current.p_no - 1].completed = current.start_when;
                done++;
            }
        } else { // This was the final CPU burst.
            local_processes[current.p_no - 1].completed = time;
            done++;
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