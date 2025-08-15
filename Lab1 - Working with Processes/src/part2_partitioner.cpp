#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

using namespace std;

int main(int argc, char **argv)
{
	if(argc != 6)
	{
		cout << getpid() << " usage: ./partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position> <max-chunk-size>\nprovided arguments:\n";
		for(int i = 0; i < argc; i++)
			cout << argv[i] << "\n";
		return -1;
	}
	
	char *file_to_search_in = argv[1];
	char *pattern_to_search_for = argv[2];
	int search_start_position = atoi(argv[3]);
	int search_end_position = atoi(argv[4]);
	int max_chunk_size = atoi(argv[5]);
	
	ifstream f(argv[1]);
	if(!f.is_open()){
		cerr << "Error: File could not be opened";
		exit(-1);
	}
	cout << "[" << getpid() << "] start position = " << search_start_position << " ; end position = " << search_end_position << "\n";
	
	string s;
	
	string patt(pattern_to_search_for);
	getline(f, s);

	int my_pid = getpid();

	int length = search_end_position - search_start_position;
	if(length > max_chunk_size)  //split into two processes
	{
		pid_t lpid = fork(), rpid;

		if(lpid == 0) {
			//exec part2_partitioner with first half

			cout << "[" << getpid() << "] is the left child and is calling part2_partitioner\n";
			char*args[7];
		       	args[0] = strdup("./part2_partitioner.out");
			args[1] = strdup("./file.txt");
			args[2] = pattern_to_search_for;
			args[3] = to_string(search_start_position).data();
			args[4] = to_string(search_start_position+length/2).data();
			args[5] = to_string(max_chunk_size).data();
			args[6] = NULL;
			execv(args[0], args);
		} else{
			cout << "[" << getpid() << "] forked left child [" << lpid << "]\n";
			
			rpid = fork();

			if(rpid == 0){
				//exec part2_partitioner with second half

				cout << "[" << getpid() << "] is the right child and is calling part2_partitioner\n";
				char*args[7];
		       		args[0] = strdup("./part2_partitioner.out");
				args[1] = strdup("./file.txt");
				args[2] = pattern_to_search_for;
				args[3] = to_string(search_start_position+length/2+1).data();
				args[4] = to_string(search_end_position).data();
				args[5] = to_string(max_chunk_size).data();
				args[6] = NULL;
				execv(args[0], args);
			} else{
				cout << "[" << getpid() << "] forked right child [" << rpid << "]\n";
				
				//parent should wait
				waitpid(lpid, NULL, 0);
				cout << "[" << getpid() << "] left child [" << lpid << "] returned\n";
				waitpid(rpid, NULL, 0);
				cout << "[" << getpid() << "] right child [" << rpid << "] returned\n";


				//cout << "[" << my_pid << "] received SIGTERM\n"; //applicable for Part III of the assignment
			}
		}
	} else{
		//exec part2_searcher
		pid_t searcher_pid = fork();
		if(searcher_pid > 0){  //parent
			cout << "[" << getpid() << "] forked searcher child [" << searcher_pid << "]\n";
			waitpid(searcher_pid, NULL, 0);
			cout << "[" << getpid() << "] searcher child [" << searcher_pid << "] returned \n";
		}
		else{  //searcher child
			string executable = "part2_searcher.out";
			char*args[6];
			args[0] = executable.data();
			args[1] = file_to_search_in;
			args[2] = pattern_to_search_for;
			args[3] = to_string(search_start_position).data();
			args[4] = to_string(search_end_position).data();
			args[5] = NULL;

			execv("part2_searcher.out", args);
		}
	}
	return 0;
}
