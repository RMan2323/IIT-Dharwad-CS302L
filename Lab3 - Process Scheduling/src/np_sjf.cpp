#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <chrono>
#include <iomanip>

#include "algos.h"

using namespace std;

struct ScheduleEntry {
    int p_no;
    int burst_no;
    int start_time;
    int end_time;
};

class comp {
   public:
    bool operator()(const Process& p1, const Process& p2) {
        return p1.bursts[p1.current] > p2.bursts[p2.current];
    }
};

void np_sjf(const vector<Process>& processes, string file_name) {
    vector<Process> local_processes = processes;
    vector<ScheduleEntry> schedule_log;

    auto sim_start_timer = chrono::high_resolution_clock::now();

    priority_queue<Process, vector<Process>, comp> ready;
    int time = local_processes.empty() ? 0 : local_processes[0].arrival;
    int not_arrived = 0;
    int done = 0;

    while (done != local_processes.size()) {
        for (; not_arrived < local_processes.size();) {
            if (local_processes[not_arrived].arrival <= time) {
                ready.push(local_processes[not_arrived]);
                not_arrived++;
            } else {
                break;
            }
        }

        //If the ready queue is empty, no process can run. Jump time to the next arrival
        if (ready.empty()) {
            if (not_arrived < local_processes.size()) {
                time = local_processes[not_arrived].arrival;
            }
            continue;
        }

        Process current = ready.top();
        ready.pop();

        if (current.start_when > time) {
            time = current.start_when;
        }

        int start_time = time;
        int duration = current.bursts[current.current];
        time += duration;

        schedule_log.push_back({current.p_no, (current.current - 1) / 2, start_time, time - 1});

        current.current++;

        if (current.current < current.bursts.size()) {
            current.start_when = time + current.bursts[current.current];
            current.current++;
            
            if (current.current < current.bursts.size()) {
                 ready.push(current);
            } else {
                local_processes[current.p_no - 1].completed = current.start_when;
                done++;
            }
        } else {
            local_processes[current.p_no - 1].completed = time;
            done++;
        }
    }

    auto sim_end_timer = chrono::high_resolution_clock::now();

    //File output
    chrono::duration<double, milli> elapsed_ms = sim_end_timer - sim_start_timer;

    cout << file_name << endl;

    ofstream schedule_file(file_name);
    if (!schedule_file.is_open()) {
        cerr << "Error: Could not open schedule.txt for writing." << endl;
        return;
    }

    schedule_file << "CPU1\n";
    for (const auto& entry : schedule_log) {
        schedule_file << "P" << entry.p_no << "," << entry.burst_no + 1
                      << "\t" << entry.start_time << "\t" << entry.end_time << "\n";
    }

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