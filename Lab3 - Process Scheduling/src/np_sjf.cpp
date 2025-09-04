#include <iostream>
#include <vector>
#include <queue>

using namespace std;

class process{
    public:
    int p_no;            //process number
    int arrival;         //arrival time
    vector<int> bursts;  //bursts
    int current;         //points to the current burst that is to be run
    int completed;       //holds the time when processes was completed, else holds -1
    int start_when;      //stores the time at/after which the process can begin its CPU burst

    process(const vector<int>& proc, const int i){
        arrival = proc[0];
        bursts = proc;
        current = 1;  //start from bursts[1] since bursts[0] is the arrival time
        completed = -1;
        p_no = i+1;
        start_when = arrival;
    }
};

class comp{
    public:
    bool operator()(const process& p1, const process& p2){
        return p1.bursts[p1.current] > p2.bursts[p2.current];
    }
};

void np_sjf(const vector<vector<int>>& procs){
    vector<process> processes;

    for(int i = 0; i < procs.size(); i++){
        process p(procs[i], i);
        processes.emplace_back(p);
    }

    for(auto i: processes){
        cout << i.arrival << endl;
    }

    priority_queue<process, vector<process>, comp> ready;
    // queue<process> blocked;

    int time = processes[0].arrival;  //processes already given with non-decreasing arrivals
    int iter = 0;
    int not_arrived = 0;  //min process no. which hasn't arrived yet
    int done = 0;
    while(done != processes.size()){
        iter++;

        //push ready processes to the queue
        // int p = 0;
        cout << "Current time: " << time << endl;;
        for(; not_arrived < processes.size();){
            // p++;
            if(processes[not_arrived].arrival <= time){
                cout << "\tProcess " << not_arrived << " is ready\n";
                ready.push(processes[not_arrived]);
                not_arrived++;
            } else break;
        }

        //pop and run the shortest ready process
        process current = ready.top();
        cout << "\tRunning P" << current.p_no << "," << (current.current-1)/2 << endl;
        time += (current.bursts[current.current]);
        current.current++;
        ready.pop();
        cout << "\tProcess ran till " << time-1 << endl << endl;

        //add the io time and push it back into ready queue
        current.start_when = time+current.bursts[current.current];
        current.current++;
        if(current.current < current.bursts.size()) ready.push(current);
        else {
            current.completed = time;
            done++;
        }
    }
}