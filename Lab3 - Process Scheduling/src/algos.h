#ifndef ALGOS
#define ALGOS

#include <vector>

using namespace std;

class Process{
    public:
    int p_no;            //process number
    int arrival;         //arrival time
    vector<int> bursts;  //bursts
    int current;         //points to the current burst that is to be run
    int completed;       //holds the time when processes was completed, else holds -1
    int start_when;      //stores the time at/after which the process can begin its CPU burst

    Process() {
        p_no = -1;
        arrival = 0;
        current = 1;
        completed = -1;
        start_when = 0;
    }

    Process(const vector<int>& proc, const int i){
        arrival = proc[0];
        bursts = proc;
        current = 1;  //start from bursts[1] since bursts[0] is the arrival time
        completed = -1;
        p_no = i+1;
        start_when = arrival;
    }
};

void fifo(const vector<Process>& processes);
void np_sjf(const vector<Process>& processes);
void np_sjf_2(const vector<Process>& processes);
void p_sjf(const vector<Process>& processes);
void rr(const vector<Process>& processes, int quantum);
void rr_2(const vector<Process>& processes, int quantum);

#endif