#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <climits>
#include <chrono>
#include <iomanip>

#include "algos.h"

using namespace std;

//struct to log schedule entries for multiple CPUs
struct ScheduleEntry2 {
    int cpu_id;
    int p_no;
    int burst_no;
    int start_time;
    int end_time;
};

struct RRItem {
    Process proc;
    int remaining;
};

struct CPU {
    bool busy = false;
    RRItem current_item;
    int finish_time = -1;
};

void rr_2(const vector<Process>& processes, int quantum, string file_name) {
    //making a mutable copy to track completion times
    vector<Process> local_processes = processes;
    vector<ScheduleEntry2> schedule_log;

    //simulation starts
    auto sim_start_timer = chrono::high_resolution_clock::now();

    queue<RRItem> ready;
    int time = local_processes.empty() ? 0 : local_processes[0].arrival;
    int not_arrived = 0;
    int done = 0;
    CPU cpus[2];

    //initial population of the ready queue
    while (not_arrived < local_processes.size() && local_processes[not_arrived].arrival <= time) {
        ready.push({local_processes[not_arrived], local_processes[not_arrived].bursts[1]});
        not_arrived++;
    }

    while (done != local_processes.size()) {
        //assigning ready jobs to free CPUs
        for (int i = 0; i < 2; i++) {
            if (!cpus[i].busy && !ready.empty()) {
                RRItem item = ready.front();
                ready.pop();
                Process current = item.proc;

                int burst_no = (current.current - 1) / 2;
                int start_time = max(time, current.start_when);
                int run_time = min(quantum, item.remaining);

                //logging the schedule entry
                schedule_log.push_back({i + 1, current.p_no, burst_no, start_time, start_time + run_time - 1});

                cpus[i].busy = true;
                cpus[i].current_item = {current, item.remaining - run_time};
                cpus[i].finish_time = start_time + run_time;
            }
        }

        //finding time of next event (earliest quantum/burst completion)
        int next_event_time = INT_MAX;
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy) next_event_time = min(next_event_time, cpus[i].finish_time);
        }

        //whem no CPUs are running, jumping time to the next process arrival
        if (next_event_time == INT_MAX) {
            if (not_arrived < local_processes.size()) {
                time = local_processes[not_arrived].arrival;
            } else { //when no more arrivals and nothing running, done
                break;
            }
        } else {
            time = next_event_time;
        }
        
        //bringing in any process that arrived while others were running
        while (not_arrived < local_processes.size() && local_processes[not_arrived].arrival <= time) {
            ready.push({local_processes[not_arrived], local_processes[not_arrived].bursts[1]});
            not_arrived++;
        }

        //processing all CPUs that finished at the current time
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy && cpus[i].finish_time == time) {
                RRItem item = cpus[i].current_item;
                Process current = item.proc;

                if (item.remaining > 0) {
                    ready.push(item); //not finished, so putting back in ready queue
                } else {
                    //burst finished
                    current.current++;
                    if (current.current < current.bursts.size()) {
                        //setting start_when for after I/O and re-queue for next burst
                        current.start_when = time + current.bursts[current.current];
                        current.current++;
                        ready.push({current, current.bursts[current.current]});
                    } else {
                        //process complete
                        local_processes[current.p_no - 1].completed = time;
                        done++;
                    }
                }
                cpus[i].busy = false;
            }
        }
    }

    auto sim_end_timer = chrono::high_resolution_clock::now();
    //simulation ends

    //printing file
    chrono::duration<double, milli> elapsed_ms = sim_end_timer - sim_start_timer;

    
    string full_file_name = file_name.substr(0, file_name.length()-4) + "_quantum_" + to_string(quantum) + ".txt";
    cout << full_file_name << endl;

    ofstream schedule_file(full_file_name);
    if (!schedule_file.is_open()) {
        cerr << "Error: Could not open schedule_2.txt for writing." << endl;
        return;
    }

    //writing the schedule from log
    for (const auto& entry : schedule_log) {
        schedule_file << "CPU" << entry.cpu_id << "\n" << "P" << entry.p_no << "," << entry.burst_no + 1
                      << "\t" << entry.start_time << "\t" << entry.end_time << "\n";
    }
    
    //calculating and writing the statistics
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