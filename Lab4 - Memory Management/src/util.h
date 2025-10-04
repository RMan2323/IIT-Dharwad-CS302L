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
    int frameNum;
    int status;  //valid bit; 0 for invalid; 1 for valid
    int timeTag;
    int pid;
    uint64_t pageNum;

    frame(){
        status = 0;
        timeTag = 0;
        pid = -1;
        pageNum = -1;
    }

    frame(int frameNo, int pid, uint64_t pageNum){
        status = 0;
        timeTag = 0;
        this->pid = pid;
        this->pageNum = pageNum;
        frameNum = frameNo;
    }
};

void fifo_local(const vector<access>& trace, const int& pageSize, const int& numFrames);
void fifo_global(const vector<access>& trace, const int& pageSize, const int& numFrames);

#endif