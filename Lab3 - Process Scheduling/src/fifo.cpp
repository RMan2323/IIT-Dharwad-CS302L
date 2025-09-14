#include <iostream>
#include <vector>
#include <queue>
#include <utility>
#include <fstream>  // Required for file output
#include "algos.h"

using namespace std;

class comp{
    public:
    bool operator()(const Process& p1, const Process& p2){
        return p1.start_when > p2.start_when;
    }
};

void fifo(const vector<Process>& processes)
{
    // --- Added for file output ---
    ofstream schedule_file("schedule.txt");
    if (!schedule_file.is_open()) {
        cerr << "Error: Could not open schedule.txt for writing." << endl;
        return;
    }
    // Write the CPU header to the file.
    schedule_file << "CPU0" << endl;
    // ---------------------------

    cout << processes.size() <<endl;
    for(auto i: processes){
        cout << i.arrival << endl;
    }

    priority_queue<Process, vector<Process>, comp> ready;
    
    int time = processes[0].arrival;  //processes already given with non-decreasing arrivals
    
    int not_arrived = 0;
    int done = 0;
    
    while(done != processes.size()){
        cout << "Current time: " << time << endl;

        for(; not_arrived < processes.size();){
            if(processes[not_arrived].arrival <= time){
                cout << "\tProcess " << not_arrived + 1 << " is ready\n";
                ready.push(processes[not_arrived]);
                not_arrived++;
            } else break;
        }

        cout << ready.top().p_no << endl;

        Process current = ready.top();
        ready.pop();

        // --- Modified for file output ---
        int start_time = time;
        int duration = current.bursts[current.current];
        time += duration;
        int end_time = time - 1;

        // Write the execution details to the schedule file
        schedule_file << "P" << current.p_no << "," << (current.current - 1) / 2 + 1
                      << "\t" << start_time << "\t" << end_time << endl;
        // -----------------------------

        cout << "\tRunning P" << current.p_no << "," << (current.current-1)/2 + 1 << endl;
        current.current++;
        cout << "\tProcess ran till " << time-1 << endl << endl;

        // If there are more bursts, schedule the next one after I/O
        if (current.current < current.bursts.size()) {
            current.start_when = time + current.bursts[current.current];
            current.current++;
            if (current.current < current.bursts.size()) {
                ready.push(current);
            } else {
                // Last burst was a CPU burst followed by a final I/O
                current.completed = current.start_when;
                done++;
            }
        } else {
            // This was the final CPU burst
            current.completed = time;
            done++;
        }
    }

    // --- Added for file output ---
    schedule_file.close();
    // ---------------------------
}