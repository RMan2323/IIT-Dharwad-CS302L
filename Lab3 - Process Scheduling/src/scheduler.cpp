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
		cout << "0) FIFO\n1) Non-Preemptive SJF\n2) Preemptive SJF\n3) Round Robin\n";
		exit(-1);
	}
	
	if(atoi(argv[1]) > 3 || atoi(argv[1]) < 0){
		cout << "Invalid algorithm\n";
		cout << "Algorithm can be one of the following:\n";
		cout << "0) FIFO\n1) Non-Preemptive SJF\n2) Preemptive SJF\n3) Round Robin\n";
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
		if(s.find('<') == string::npos){  //if "<" is not found
			stringstream ss(s);           //convert line to stream
			string p;                     //store the tokens in this before appending to vec
			vector<int> process;          //temp vec to store tokens
			while(ss >> p){               //extract tokens by whitespace into p
				if(p == "-1") break;
				process.push_back(stoi(p));
			}

			processes.push_back(process);
		}

		if(strstr(s.data(), end.data()))  //if "</pre>" is found
			break;
	}

	for(auto i:processes){
		for(auto j:i){
			cout << j << " ";
		}
		cout << endl;
	}

	cout << endl;

	vector<Process> process_arr;	//vector of process objects

    for(int i = 0; i < processes.size(); i++){
        Process p(processes[i], i); //convert process number i to process object
        process_arr.push_back(p);   //push into the queue
    }

// In scheduler.cpp, inside the switch case for FIFO

	switch(atoi(argv[1])){
		case 0:
		// For Part I (1 CPU)
		cout << "\n--- Running FIFO for 1 CPU ---" << endl;
		fifo(process_arr); 

		// For Part II (2 CPUs)
		cout << "\n--- Running FIFO for 2 CPUs ---" << endl;
		fifo_2(process_arr);
		break;


		case 1:
			np_sjf(process_arr);
			np_sjf_2(process_arr);
			break;

		case 2:
			//TODO preemptive_sjf
			break;

		case 3:
			int quantum;
			cout << "Enter time quantum: ";
			cin >> quantum;
			rr(process_arr, quantum);
			rr_2(process_arr, quantum);
			break;
	}
    
    return 0;
}
