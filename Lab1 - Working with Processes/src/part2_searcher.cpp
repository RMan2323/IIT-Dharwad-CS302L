#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <signal.h>

using namespace std;

int main(int argc, char **argv){
	cout << "[" << getpid() << "] will start searching!\n";
	if(argc != 5){
		cout <<"usage: ./partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position>\nprovided arguments:\n";
		for(int i = 0; i < argc; i++)
			cout << argv[i] << "\n";
		return -1;
	}
	
	ifstream f(argv[1]);
	if(!f.is_open()){
		cerr << "Error: File could not be opened";
		exit(-1);
	}
	
	string s;
	
	char *file_to_search_in = argv[1];
	char *pattern_to_search_for = argv[2];
	int search_start_position = atoi(argv[3]);
	int search_end_position = atoi(argv[4]);
	
	string patt(pattern_to_search_for);
	getline(f, s);

	for(int i = search_start_position; i < search_end_position; i++){
		for(int j = 0; j < patt.length(); j++){
			if(patt[j] != s[i+j]){
				break;
			}
			if(j == patt.length()-1 && patt[j] == s[i+j]){
				cout << "[" << getpid() << "] found at " << i << "\n";
				return 0;
			}
		}
	}
	
	cout << "[" << getpid() << "] didn't find\n";
	return 0;
}

