#ifndef UTIL
#define UTIL

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

class pageTable{
    public:
    tuple<int, int, frame>* mapping;  //pid,pageNum -> frameNum

    pageTable(int pageSize){
        
    }
};

#endif
