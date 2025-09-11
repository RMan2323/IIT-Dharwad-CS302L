#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include "algos.h"

using namespace std;

struct RRItem {
    Process proc;
    int remaining;  //remaining burst time
};

void rr(const vector<Process>& processes, int quantum) {
    ofstream f("./schedule.txt");

    queue<RRItem> ready;
    int time = processes[0].arrival;
    int not_arrived = 0;
    int done = 0;

    while (not_arrived < processes.size() && processes[not_arrived].arrival <= time) {
        ready.push({processes[not_arrived], processes[not_arrived].bursts[1]});
        not_arrived++;
    }

    while (done != processes.size()) {
        if (ready.empty()) {
            //jump to next arrival
            if (not_arrived < processes.size()) {
                time = processes[not_arrived].arrival;
                ready.push({processes[not_arrived], processes[not_arrived].bursts[1]});
                not_arrived++;
            }
            continue;
        }

        RRItem item = ready.front();
        ready.pop();
        Process current = item.proc;

        int burst_no = (current.current - 1) / 2;
        int start_time = max(time, current.start_when);
        int run_time = min(quantum, item.remaining);

        f << "P" << current.p_no << "," << burst_no << "\t" << start_time << "\t" << (start_time + run_time - 1) << "\n";

        time = start_time + run_time;
        item.remaining -= run_time;

        if (item.remaining > 0) {
            //put back into queue
            item.proc = current;
            ready.push(item);
        } else {
            //burst finished
            current.current++;
            if (current.current < current.bursts.size()) {
                //add IO
                current.start_when = time + current.bursts[current.current];
                current.current++;
                ready.push({current, current.bursts[current.current]});
            } else {
                current.completed = time - 1;
                done++;
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