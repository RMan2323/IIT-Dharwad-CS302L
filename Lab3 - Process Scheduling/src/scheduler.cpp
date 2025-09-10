#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include "algos.h"

using namespace std;

int main(int argc, char **argv){

	if(argc != 2){
		cout << "Usage: ./a.out <path to .dat file>\n";
		exit(0);
	}

    ifstream f(argv[1]);
	if(!f.is_open()){
		cerr << "Error: File could not be opened";
		exit(-1);
	}

    string s;
	string start = "<", end = "</pre>";
	vector<vector<int>> processes;

	while(getline(f, s)){
		if(s.find('<') == string::npos){  //if "<" is not found
			stringstream ss(s);           //convert line to stream
			string p;                     //store the tokens in this before appending to vec
			vector<int> Process;          //vec to store tokens
			while(ss >> p){               //extract tokens by whitespace into p
				if(p == "-1") break;
				Process.push_back(stoi(p));
			}

			processes.push_back(Process);
		}

		if(strstr(s.data(), end.data()))  //if "</pre>" is found
			break;
	}

	// cout << "check_1" << endl;


	for(auto i:processes){
		for(auto j:i){
			cout << j << " ";
		}
		cout << endl;
	}

	cout << endl;

	// cout << "check_2" << endl;

	vector<Process> process_arr;

	// cout << "check_3" << endl;

    for(int i = 0; i < processes.size(); i++){
        Process p(processes[i], i);
        process_arr.push_back(p);
    }

	// cout << "check_4" << endl;

	// np_sjf(process_arr);
	fifo(process_arr);
    
    return 0;
}
