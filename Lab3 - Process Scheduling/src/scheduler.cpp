#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include "algos.h"

using namespace std;

int main(int argc, char **argv){

	if(argc != 3){
		cout << "Usage: ./a.out <algorithm> <path to .dat file>\n";
		cout << "Algorithm can be one of the following:\n";
		cout << "1) FIFO\n2) Non-Preemptive SJF\n3) Preemptive SJF\n4) Round Robin\n";
		exit(-1);
	}
	
	if(atoi(argv[1]) > 4 || atoi(argv[1]) < 1){
		cout << "Invalid algorithm\n";
		cout << "Algorithm can be one of the following:\n";
		cout << "1) FIFO\n2) Non-Preemptive SJF\n3) Preemptive SJF\n4) Round Robin\n";
		exit(-1);
	}

	ifstream f(argv[2]);
	if(!f.is_open()){
		cerr << "Error: File could not be opened";
		exit(-1);
	}

    string s;
	string start = "<", end = "</pre>";
	vector<vector<int>> processes;        //2D int arr to store .dat file

	while(getline(f, s)){
		if(s.find('<') == string::npos){
			stringstream ss(s);           //convert line to stream
			string p;                     //store the tokens in this before appending to vec
			vector<int> process;          //temp vec to store tokens
			while(ss >> p){               //extract tokens by whitespace into p
				if(p == "-1") break;
				process.push_back(stoi(p));
			}

			processes.push_back(process);
		}

		if(strstr(s.data(), end.data()))
			break;
	}

	for(auto i:processes){
		for(auto j:i){
			cout << j << " ";
		}
		cout << endl;
	}

	cout << endl;

	vector<Process> process_arr;

    for(int i = 0; i < processes.size(); i++){
        Process p(processes[i], i);
        process_arr.push_back(p); 
    }


	string process_file = argv[2];

	switch(atoi(argv[1])){
		case 1:
		cout << "\n--- Running FIFO for 1 CPU ---" << endl;
		fifo(process_arr, "schedule_fifo_" + process_file.substr(2, process_file.length() - 6) + ".txt"); 

		cout << "\n--- Running FIFO for 2 CPUs ---" << endl;
		fifo_2(process_arr, "schedule_fifo_2_" + process_file.substr(2, process_file.length() - 6) + ".txt");
		break;


		case 2:
			np_sjf(process_arr, "schedule_np_sjf_" + process_file.substr(2, process_file.length() - 6) + ".txt");
			np_sjf_2(process_arr, "schedule_np_sjf_2_" + process_file.substr(2, process_file.length() - 6) + ".txt");
			break;

		case 3:
        cout << "\n--- Running Preemptive SJF for 1 CPU ---" << endl;
        p_sjf(process_arr, "schedule_p_sjf_" + process_file.substr(2, process_file.length() - 6) + ".txt");

        for(auto& p : process_arr) {
            p.current = 1;
            p.completed = -1;
            p.start_when = p.arrival;
            p.remaining_time = 0;
        }

        cout << "\n--- Running Preemptive SJF for 2 CPUs ---" << endl;
        p_sjf_2(process_arr, "schedule_p_sjf_2_" + process_file.substr(2, process_file.length() - 6) + ".txt");
        break;

		case 4:
			int quantum;
			cout << "Enter time quantum: ";
			cin >> quantum;
			rr(process_arr, quantum, "schedule_rr_" + process_file.substr(2, process_file.length() - 6) + ".txt");
			rr_2(process_arr, quantum, "schedule_rr_2_" + process_file.substr(2, process_file.length() - 6) + ".txt");
			break;
	}
    
    return 0;
}
