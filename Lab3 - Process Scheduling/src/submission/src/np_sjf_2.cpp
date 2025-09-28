#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <climits>
#include <chrono>
#include <iomanip>

#include "algos.h"

using namespace std;

struct ScheduleEntry2 {
    int cpu_id;
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

class CPU {
   public:
    bool busy = false;
    Process current_process;
    int finish_time = -1;
};

void np_sjf_2(const vector<Process>& processes, string file_name) {
    vector<Process> local_processes = processes;
    vector<ScheduleEntry2> schedule_log;

    auto sim_start_timer = chrono::high_resolution_clock::now();

    priority_queue<Process, vector<Process>, comp> ready;
    int time = local_processes.empty() ? 0 : local_processes[0].arrival;
    int not_arrived = 0;
    int done = 0;
    CPU cpus[2];

    while (done != local_processes.size()) {
        //Add arrived processes to the ready queue
        while (not_arrived < local_processes.size() && local_processes[not_arrived].arrival <= time) {
            ready.push(local_processes[not_arrived]);
            not_arrived++;
        }

        //Assign ready jobs to any free CPU
        for (int i = 0; i < 2; i++) {
            if (!cpus[i].busy && !ready.empty()) {
                Process p = ready.top();
                ready.pop();

                int start_time = (p.start_when > time) ? p.start_when : time;
                int duration = p.bursts[p.current];

                cpus[i].busy = true;
                cpus[i].finish_time = start_time + duration;
                cpus[i].current_process = p;

                schedule_log.push_back({i + 1, p.p_no, (p.current - 1) / 2, start_time, cpus[i].finish_time - 1});
            }
        }

        //Find the earliest time a CPU will finish
        int next_event_time = INT_MAX;
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy) {
                next_event_time = min(next_event_time, cpus[i].finish_time);
            }
        }

        //If no CPUs are busy, jump time to the next process arrival
        if (next_event_time == INT_MAX) {
            if (not_arrived < local_processes.size()) {
                time = local_processes[not_arrived].arrival;
            }
            continue;
        }

        time = next_event_time;

        //Process all CPUs that finished at this time
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy && cpus[i].finish_time == time) {
                Process p = cpus[i].current_process;
                p.current++;

                if (p.current < p.bursts.size()) {
                    p.start_when = time + p.bursts[p.current];
                    p.current++;
                    if (p.current < p.bursts.size()) {
                        ready.push(p);
                    } else {
                        local_processes[p.p_no - 1].completed = p.start_when;
                        done++;
                    }
                } else {
                    local_processes[p.p_no - 1].completed = time;
                    done++;
                }
                cpus[i].busy = false;
            }
        }
    }
    
    auto sim_end_timer = chrono::high_resolution_clock::now();

    //File output
    chrono::duration<double, milli> elapsed_ms = sim_end_timer - sim_start_timer;
    
    cout << file_name << endl;

    ofstream schedule_file(file_name);
    if (!schedule_file.is_open()) {
        cerr << "Error: Could not open schedule_2.txt for writing." << endl;
        return;
    }

    for (const auto& entry : schedule_log) {
        schedule_file << "CPU" << entry.cpu_id << "\n" << "P" << entry.p_no << "," << entry.burst_no + 1
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