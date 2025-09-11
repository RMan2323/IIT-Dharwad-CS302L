#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include "algos.h"

using namespace std;

class comp{
    public:
    bool operator()(const Process& p1, const Process& p2){
        return p1.bursts[p1.current] > p2.bursts[p2.current];
    }
};

void np_sjf(const vector<Process>& processes){
    ofstream f;
    f.open("./schedule.txt");

    // cout << processes.size() <<endl;
    // for(auto i: processes){
    //     cout << i.arrival << endl;
    // }

    priority_queue<Process, vector<Process>, comp> ready;
    
    int time = processes[0].arrival;  //processes already given with non-decreasing arrivals
    int iter = 0;
    int not_arrived = 0;  //min Process no. which hasn't arrived yet
    int done = 0;

    while(done != processes.size()){
        iter++;

        cout << "Current time: " << time << endl;;
        for(; not_arrived < processes.size();){
            if(processes[not_arrived].arrival <= time){
                cout << "\tProcess " << not_arrived << " is ready\n";
                ready.push(processes[not_arrived]);
                not_arrived++;
            } else break;
        }

        //pop and run the shortest ready process
        Process current = ready.top();
        if(current.start_when > time) time = current.start_when;
        cout << "\tRunning P" << current.p_no << "," << (current.current-1)/2 << endl;
        f << "P" << current.p_no << "," << (current.current-1)/2 << "\t" << time << "\t";
        time += (current.bursts[current.current]);
        current.current++;
        ready.pop();
        cout << "\tProcess ran till " << time-1 << endl << endl;


        //add the io time and push it back into ready queue
        current.start_when = time+current.bursts[current.current];
        current.current++;
        if(current.current < current.bursts.size()) ready.push(current);
        else {
            current.completed = time-1;
            done++;
        }
        f << time-1 << "\n";
    }
    f.close();
}