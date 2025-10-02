#include <iostream>
#include <vector>
#include <queue>
#include <utility>
#include <math.h>
#include "util.h"

class pageTable{
    vector<pair<int, int>> table;  //pageNum -> frameNum
    uint64_t maxLength;

    public:
    pageTable(int numFrames){
        //number of entries = 2^64/pageSize
        //maximum length requirement is 2^64/64 = 2^58
        //however, we simplify the simulation by making it similar to a inverted page table
        //maximum number of entries are numFrames, rest go to swapSpace
        maxLength = numFrames;
    }

    void enque(int pageNum){
        int frameNum = -1;

        //iterate through memory and find a frame
        for(int i = 0; i < maxLength; i++){  //maxLength is same as numFrames
            if(physicalMemory[i].status == 0){
                physicalMemory[i].status = 1;
                frameNum = i;
            }
        }

        //if no free frame found, evict page at table[0] and allocate its frame
        if(frameNum == -1){
            frameNum = table[0].second;  //frame number of the first allocated frame
            table.erase(table.begin());
        }

        table.emplace_back(pageNum, frameNum);
    }

    //returns frameNum if found, else -1;
    int findFrame(int pageNum){
        for(int i = 0; i < table.size(); i++){
            if(table[i].first == pageNum){
                return table[i].second;
            }
        }
        return -1;
    }
};

void fifo_local(const vector<access>& trace, const int& pageSize, const int& numFrames, frame* physicalMemory){
    int faults = 0;
    vector<pageTable> pTable;
    for(int i = 0; i < 4; i++){
        pTable.emplace_back(numFrames);
    }

    int pageBits = 64-log2(pageSize);
    int pageNum;
    for(access acc: trace){
        //get the page number
        //find it in table
        //if not found, increase fault and push it into the table

        //number of upper bits required = log_2(2^64/pageSize) = 64-log_2(pageSize)

        pageNum = acc.va >> (64-pageBits);
        if(pTable[acc.pid].findFrame(pageNum) == -1){
            faults++;
            pTable[acc.pid].enque(pageNum);
        }
    }
}