#include <iostream>
#include <fstream>
#include <vector>
#include "util.h"

using namespace std;

int main(int argc, char* argv[]){
    if(argc != 6){
        cerr << "Usage: ./simulator.out <page size in bytes> <number of memory frames> <replacement policy> <allocation policy> <path to memory trace file>\n";

        cerr << "Page size must be between 64 B and 64 KB\n";

        cerr << "Number of memory frames must be between 4 and 64K\n";

        cerr << "Replacement policy must be one of the following:\n" << "1) Optimal\n2) FIFO\n3) LRU\n4) Random\n";

        cerr << "Allocation policy must be one of the following:\n" << "1) Global\n2) Local\n";

        return 1;
    }

    int pageSize = stoi(argv[1]);
    int numFrames = stoi(argv[2]);
    int repPolicy = stoi(argv[3]);
    int allocPolicy = stoi(argv[4]);

    if(64 > pageSize || pageSize > 65536){
        cerr << "Page size must be between 64 B and 64 KB\n";

        return 1;
    }
    if(4 > numFrames || numFrames > 64000){
        cerr << "Number of frames must be between 4 and 64K\n";

        return 1;
    }
    if(1 > repPolicy || repPolicy > 4){
        cerr << "Replacement policy must be one of the following:\n" << "1) Optimal\n2) FIFO\n3) LRU\n4) Random\n";

        return 1;
    }
    if(1 > allocPolicy || allocPolicy > 2){
        cerr << "Allocation policy must be one of the following:\n" << "1) Global\n2) Local\n";

        return 1;
    }

    ifstream file(argv[5]);

    if(!file.is_open()){
        cerr << "File " << argv[5] << " could not be opened\n";

        return 1;
    }

    vector<access> trace;

    string line;
    access ac;
    while(getline(file, line, ',')){
        // cout << "PID " << line << ", Addr ";
        ac.pid = (int)stoi(line);
        getline(file, line);
        // cout << line << "\n";
        ac.va = (uint64_t)stoull(line);

        trace.emplace_back(ac);
    }
    // cout << "\n";

    file.close();

    int page_fault_count = 0;

    // Use the command-line arguments to select which simulation to run.
    if (allocPolicy == 1) { // Global Allocation
        if (repPolicy == 1) { // Optimal
            cout << "Optimal Global" << endl;
            page_fault_count = optimal_global::optimal_global(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        } else if (repPolicy == 2) { // FIFO
            cout << "FIFO Global" << endl;
            page_fault_count = fifo_global::fifo_global(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        } else if (repPolicy == 3) { // LRU
            cout << "LRU Global" << endl;
            page_fault_count = lru_global::lru_global(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        } else if (repPolicy == 4) { // Random
            cout << "Random Global" << endl;
            page_fault_count = random_global::random_global(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        }
    } else { // allocPolicy == 2 -> Local Allocation
        if (repPolicy == 1) { // Optimal
            cout << "Optimal Local" << endl;
            page_fault_count = optimal_local::optimal_local(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        } else if (repPolicy == 2) { // FIFO
            cout << "FIFO Local" << endl;
            page_fault_count = fifo_local::fifo_local(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        } else if (repPolicy == 3) { // LRU
            cout << "LRU Local" << endl;
            page_fault_count = lru_local::lru_local(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        } else if (repPolicy == 4) { // Random
            cout << "Random Local" << endl;
            page_fault_count = random_local::random_local(trace, pageSize, numFrames);
            cout << "Total: " << page_fault_count << endl;
        }
    }

    return 0;
}
