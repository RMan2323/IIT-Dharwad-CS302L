#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <climits>
#include "algos.h"

using namespace std;

class comp {
public:
    bool operator()(const Process& p1, const Process& p2) {
        return p1.bursts[p1.current] > p2.bursts[p2.current];
    }
};

class CPU {
public:
    bool busy = false;
    Process current;
    int finish_time = -1; //when the current burst ends
    int start_time = -1;
    int burst_no = -1;

    CPU(){
        busy = false;
        finish_time = -1;
        start_time = -1;
        burst_no = -1;
    }
};

void np_sjf_2(const vector<Process>& processes) {
    ofstream f("./schedule_2.txt");

    priority_queue<Process, vector<Process>, comp> ready;

    int time = processes[0].arrival; //processes already sorted by arrival
    int not_arrived = 0;
    int done = 0;

    CPU cpus[2]; //two cores

    while (done != processes.size()) {
        while (not_arrived < processes.size() && processes[not_arrived].arrival <= time) {
            ready.push(processes[not_arrived]);
            not_arrived++;
        }

        //give ready jobs to any free CPU
        for (int i = 0; i < 2; i++) {
            if (!cpus[i].busy && !ready.empty()) {
                Process current = ready.top();
                ready.pop();

                if (current.start_when > time) time = current.start_when;

                cpus[i].busy = true;
                cpus[i].start_time = time;
                cpus[i].burst_no = (current.current - 1) / 2;
                cpus[i].finish_time = time + current.bursts[current.current];
                current.current++;

                f << "CPU " << i << "\n";
                f << "P" << current.p_no << "," << cpus[i].burst_no << "\t" << cpus[i].start_time << "\t" << (cpus[i].finish_time - 1) << "\n";

                cpus[i].current = current;
            }
        }

        //advance time to the earliest finishing CPU
        int next_event = INT_MAX;
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy) next_event = min(next_event, cpus[i].finish_time);
        }
        if (next_event == INT_MAX) {
            //nothing running, jump to next arrival
            if (not_arrived < processes.size())
                time = processes[not_arrived].arrival;
            continue;
        }

        time = next_event;

        //free CPUs that finished at this time
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy && cpus[i].finish_time == time) {
                Process current = cpus[i].current;
                //handle I/O burst if present
                if (current.current < current.bursts.size()) {
                    current.start_when = time + current.bursts[current.current];
                    current.current++;
                    if (current.current < current.bursts.size()) {
                        ready.push(current);
                    } else {
                        current.completed = time - 1;
                        done++;
                    }
                } else {
                    current.completed = time - 1;
                    done++;
                }
                cpus[i].busy = false;
            }
        }
    }
    f.close();
}