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
    int frameNum;  //frame number
    int status;  //valid bit; 0 for invalid; 1 for valid
};

class pageTable{
    public:
    tuple<int, int, frame>* mapping;  //pid,pageNum -> frameNum

    pageTable(int pageSize){
        //64 bit system
        //number of pages = 2^64 bytes/pageSize
        // = 18446744073709551616/pageSize

        //1ULL = unsigned long long (64 bit)
        //1ULL << 63 = 2^63

        double numPages = (double)(1ULL << 63) * 2.0 / (double)pageSize;
        mapping = new tuple<int, int, frame>[(1ULL << 64)/numPages];
    }
};

#endif