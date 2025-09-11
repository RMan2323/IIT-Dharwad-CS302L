#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <climits>
#include "algos.h"

using namespace std;

struct RRItem {
    Process proc;
    int remaining;
};

struct CPU {
    bool busy = false;
    RRItem current;
    int finish_time = -1;
    int burst_no = -1;
};

void rr_2(const vector<Process>& processes, int quantum) {
    ofstream f("./schedule_2.txt");

    queue<RRItem> ready;
    int time = processes[0].arrival;
    int not_arrived = 0;
    int done = 0;

    CPU cpus[2];

    //bring in first arrivals
    while (not_arrived < processes.size() && processes[not_arrived].arrival <= time) {
        ready.push({processes[not_arrived], processes[not_arrived].bursts[1]});
        not_arrived++;
    }

    while (done != processes.size()) {
        //give ready jobs to free CPUs
        for (int i = 0; i < 2; i++) {
            if (!cpus[i].busy && !ready.empty()) {
                RRItem item = ready.front();
                ready.pop();
                Process current = item.proc;

                int burst_no = (current.current - 1) / 2;
                int start_time = max(time, current.start_when);
                int run_time = min(quantum, item.remaining);

                f << "CPU " << i << "\n";
                f << "P" << current.p_no << "," << burst_no << "\t" << start_time << "\t" << (start_time + run_time - 1) << "\n";

                cpus[i].busy = true;
                cpus[i].current = {current, item.remaining - run_time};
                cpus[i].finish_time = start_time + run_time;
                cpus[i].burst_no = burst_no;
            }
        }

        //find next event
        int next_event = INT_MAX;
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy) next_event = min(next_event, cpus[i].finish_time);
        }
        if (next_event == INT_MAX) {
            //no one running, jump to next arrival
            if (not_arrived < processes.size()) {
                time = processes[not_arrived].arrival;
                ready.push({processes[not_arrived], processes[not_arrived].bursts[1]});
                not_arrived++;
            }
            continue;
        }

        time = next_event;

        //free CPUs finishing now
        for (int i = 0; i < 2; i++) {
            if (cpus[i].busy && cpus[i].finish_time == time) {
                RRItem item = cpus[i].current;
                Process current = item.proc;

                if (item.remaining > 0) {
                    ready.push(item); //back in queue
                } else {
                    //finished burst
                    current.current++;
                    if (current.current < current.bursts.size()) {
                        current.start_when = time + current.bursts[current.current];
                        current.current++;
                        ready.push({current, current.bursts[current.current]});
                    } else {
                        current.completed = time - 1;
                        done++;
                    }
                }
                cpus[i].busy = false;
            }
        }

        //bring in new arrivals
        while (not_arrived < processes.size() && processes[not_arrived].arrival <= time) {
            ready.push({processes[not_arrived], processes[not_arrived].bursts[1]});
            not_arrived++;
        }
    }

    f.close();
}