#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>

using namespace std;

int main(int argc, char **argv)
{
	if(argc != 6)
	{
		cout <<"usage: ./partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position> <max-chunk-size>\nprovided arguments:\n";
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
	
	string s;
	
	string patt(pattern_to_search_for);
	getline(f, s);

	int my_pid = getpid();

	int length = search_end_position - search_start_position;
	if(length > max_chunk_size)  //split into two processes
	{
		pid_t lpid = fork(), rpid;
		cout << "[" << my_pid << "] forked left child " << lpid << "\n";

		if(lpid == 0) {
			//exec part2_partitioner with first half
			//./partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position> <max-chunk-size>

			argv[4] = to_string(search_start_position + length/2).data();
			argv = (char**)realloc(argv, 7*sizeof(char*));
			argv[6] = NULL;
			execv(argv[0], argv);
		} else{
			rpid = fork();
			cout << "[" << my_pid << "] forked right child " << rpid << "\n";

			if(rpid == 0){
				//exec part2_partitioner with second half
				argv[3] = to_string(search_start_position + length/2 + 1).data();
				argv = (char**)realloc(argv, 7*sizeof(char*));
				argv[6] = NULL;
				execv(argv[0], argv);
			} else{
				//parent should wait
				cout << "[" << my_pid << "] start position = " << search_start_position << " ; end position = " << search_end_position << "\n";

				waitpid(lpid, NULL, 0);
				cout << "[" << my_pid << "] left child returned\n";
				waitpid(rpid, NULL, 0);
				cout << "[" << my_pid << "] right child returned\n";

				// cout << "[" << my_pid << "] left child returned\n";
				// cout << "[" << my_pid << "] right child returned\n";

				cout << "[" << my_pid << "] searcher child returned \n";

				//cout << "[" << my_pid << "] received SIGTERM\n"; //applicable for Part III of the assignment
			}
		}
	} else{
		//exec part1_searcher
		pid_t searcher_pid = fork();
		if(searcher_pid > 0){
			cout << "[" << my_pid << "] forked searcher child " << searcher_pid << "\n";
		}
		else{
			string executable = "part1.out";
			argv[0] = executable.data();
			argv[1] = file_to_search_in;
			argv[2] = pattern_to_search_for;
			argv[3] = to_string(search_start_position).data();
			argv[4] = to_string(search_end_position).data();
			argv[5] = NULL;

			execv("part2.out", argv);
		}
	}
	return 0;
}
