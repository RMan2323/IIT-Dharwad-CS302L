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
        pageNum = 0;
        frameNum = -1;
    }

    frame(int frameNum){
        status = 0;
        timeTag = 0;
        pid = -1;
        pageNum = 0;
        this->frameNum = frameNum;
    }
};

namespace fifo_local{
    void fifo_local(const vector<access>& trace, const int& pageSize, const int& numFrames);
}
namespace fifo_global{
    void fifo_global(const vector<access>& trace, const int& pageSize, const int& numFrames);
}
namespace lru_global
{
    void lru_global(const vector<access>& trace, const int& pageSize, const int& numFrames);
}

#endif