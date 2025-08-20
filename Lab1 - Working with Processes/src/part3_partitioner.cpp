#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

using namespace std;

void sighandler(int unused)
{
	//TODO the leader should wait for left and right children to exit before exiting itself
	//wait() will return -1 if there are no ongoing child processes
	while(wait(NULL) > 0);
	cout << "[" << getpid() << "] received SIGTERM\n";
	exit(0);
}

int main(int argc, char **argv)
{
	//set the pgid of this process (pid = 0) to getpgrp()
	//for the first call, getpgrp() = pgid of this pid = pgid of root
	//for subsequent execs, getpgrp() = pgid of this pid = pgid of root since pgid of children is set to pgid of root later after forking
	setpgid(0, getpgrp());
	
	if(argc != 6)
	{
		cout << getpid() << " usage: ./partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position> <max-chunk-size>\nprovided arguments:\n";
		for(int i = 0; i < argc; i++)
			cout << argv[i] << "\n";
		return -1;
	}
	
	struct sigaction sa;
	sa.sa_handler = sighandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	
	sigaction(SIGTERM, &sa, nullptr);
	
	char *file_to_search_in = argv[1];
	char *pattern_to_search_for = argv[2];
	int search_start_position = atoi(argv[3]);
	int search_end_position = atoi(argv[4]);
	int max_chunk_size = atoi(argv[5]);
	
	ifstream f(argv[1]);
	if(!f.is_open()){
		cerr << "Error: File could not be opened\n";
		exit(-1);
	}
	cout << "[" << getpid() << "] start position = " << search_start_position << " ; end position = " << search_end_position << " ; pgrp = " << getpgrp() << "\n";
	
	string s;
	
	string patt(pattern_to_search_for);
	getline(f, s);

	int my_pid = getpid();

	int length = search_end_position - search_start_position;
	if(length > max_chunk_size)  //split into two processes
	{
		pid_t lpid = fork(), rpid;

		if(lpid == 0) {  //left child
			//exec part2_partitioner with first half

			cout << "[" << getpid() << "] is the left child and is calling part3_partitioner\n";
			char*args[7];
		    args[0] = strdup("./part3_partitioner.out");
			args[1] = file_to_search_in;
			args[2] = pattern_to_search_for;
			args[3] = to_string(search_start_position).data();
			args[4] = to_string(search_start_position+length/2).data();
			args[5] = to_string(max_chunk_size).data();
			args[6] = NULL;
			execv(args[0], args);
		} else{  //parent
			setpgid(lpid, getpgrp());  //setpgid of left child to pgid of the parent process
			cout << "[" << getpid() << "] forked left child [" << lpid << "]\n";
			
			rpid = fork();

			if(rpid == 0){  //right child
				//exec part2_partitioner with second half

				cout << "[" << getpid() << "] is the right child and is calling part3_partitioner\n";
				char*args[7];
		       	args[0] = strdup("./part3_partitioner.out");
				args[1] = file_to_search_in;
				args[2] = pattern_to_search_for;
				args[3] = to_string(search_start_position+length/2+1).data();
				args[4] = to_string(search_end_position).data();
				args[5] = to_string(max_chunk_size).data();
				args[6] = NULL;
				execv(args[0], args);
			} else{  //parent
				setpgid(rpid, getpgrp());  //setpgid of right child to pgid of the parent process
				cout << "[" << getpid() << "] forked right child [" << rpid << "]\n";
				
				//parent should wait for both the processes to exit
				//cannot use a single wait() because if any one process exits, the parent may exit without all processes exiting
				//wait() will return -1 if there are no ongoing child processes
				while(wait(NULL) > 0);
				
				//int lstatus, rstatus;
				//int lwc = waitpid(lpid, &lstatus, 0), rwc = waitpid(rpid, &rstatus, 0);
				//cout << "[" << lwc << "] exited with status " << WEXITSTATUS(lstatus) << "\n";
				//cout << "[" << rwc << "] exited with status " << WEXITSTATUS(rstatus) << "\n";
			}
		}
	} else{
		//exec part3_searcher
		pid_t searcher_pid = fork();
		setpgid(searcher_pid, getpgrp());
		if(searcher_pid > 0){  //parent
			cout << "[" << getpid() << "] forked searcher child [" << searcher_pid << "]\n";
			int searchStatus;
			waitpid(searcher_pid, &searchStatus, 0);
			cout << "[" << getpid() << "] searcher child [" << searcher_pid << "] returned \n";
		}
		else{  //searcher child
			string executable = "part3_searcher.out";
			char*args[6];
			args[0] = executable.data();
			args[1] = file_to_search_in;
			args[2] = pattern_to_search_for;
			args[3] = to_string(search_start_position).data();
			args[4] = to_string(search_end_position).data();
			args[5] = NULL;
			execv("part3_searcher.out", args);
		}
	}
	return 0;
}
