#ifndef UTIL
#define UTIL

#include <iostream>
#include <vector>
#include <cstdint>
#include <tuple>
using namespace std;

class access{
    public:
    int pid;  //process number
    uint64_t va;  //virtual address
};

class frame{
    public:
    int status;  //valid bit; 0 for invalid; 1 for valid

    frame(){
        status = 0;
    }
};

extern frame* physicalMemory;  //create a global physicalMemory

// class pageTable{
//     public:
//     vector<tuple<int, int, int>> mapping;  //pageNum -> frameNum

//     void addMapping(int pid, int pageNum, int frameNo){
//         //TODO When there is need to add a new mapping
//         return;
//     }

//     int findMapping(int pid, int pageNum){
//         for(const auto& entry: mapping){
//             if (get<0>(entry) == pid && get<1>(entry) == pageNum){
//                 return get<2>(entry);
//             }
//         }
//         cout << "Page Fault" << endl;
//         //handle_page_fault TODO
//     }

//     void printMappings(){
//         if(mapping.empty()){
//             cout << "Page table is empty.\n";
//             return;
//         }

//         cout << "Page Table Mappings:" << endl;

//         for (const auto& entry : mapping){
//             cout << "PID: " << get<0>(entry)
//                  << ", PageNum: " << get<1>(entry)
//                  << " -> Frame: " << get<2>(entry) << endl;
//         }
//     }
// };

void fifo_local(const vector<access>& trace, const int& pageSize, const int& numFrames, frame* physicalMemory);

#endif