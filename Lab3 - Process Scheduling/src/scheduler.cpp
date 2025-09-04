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
			vector<int> process;          //vec to store tokens
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

	np_sjf(processes);
    
    return 0;
}