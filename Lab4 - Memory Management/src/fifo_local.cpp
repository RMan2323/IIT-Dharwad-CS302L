#include <iostream>
#include <vector>
#include <queue>
#include <utility>
#include <math.h>
#include "util.h"

class pageTable{
    vector<pair<uint64_t, int>> table;  //pageNum -> frameNum
    int maxLength;
    frame* physicalMemory;

    public:
    pageTable(int numFrames, frame* physicalMemory){
        //number of entries = 2^64/pageSize
        //maximum length requirement is 2^64/64 = 2^58
        //however, we simplify the simulation by making it similar to a inverted page table
        //maximum number of entries are numFrames, rest go to swapSpace
        maxLength = numFrames;
        this->physicalMemory = physicalMemory;
    }

    void printTable(){
        cout << "--- Table is: ---\n";
        for(pair<uint64_t, int> p: table){
            cout << p.first << "->" << p.second << "\n";
        }
        cout << "--- End of table ---\n\n";
    }

    void enque(uint64_t pageNum){
        int frameNum = -1;

        //iterate through memory and find a frame
        for(int i = 0; i < maxLength; i++){  //maxLength is same as numFrames
            if(physicalMemory[i].status == 0){
                physicalMemory[i].status = 1;
                frameNum = i;
                break;
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
    int findFrame(uint64_t pageNum){
        for(int i = 0; i < table.size(); i++){
            if(table[i].first == pageNum){
                return table[i].second;
            }
        }
        return -1;
    }
};

void fifo_local(const vector<access>& trace, const int& pageSize, const int& numFrames){
    int faults = 0;

    frame* physicalMemory = new frame[numFrames];  //physical memory implemented simply as an array of frames

    vector<pageTable> pTable;
    for(int i = 0; i < 4; i++){
        pTable.emplace_back(numFrames, physicalMemory);
    }

    // int pageBits = 64-static_cast<uint64_t>(log2(pageSize));  //log2(pageSize) is offset, rest are pageBits
    uint64_t pageNum;

    // cout << "pageBits = upper " << pageBits << "\n";

    for(access acc: trace){
        // for(int i = 0; i < numFrames; i++)  //maxLength is same as numFrames
        //     cout << "Frame " << i << " is " << physicalMemory[i].status << "\n";
        
        //get the page number
        //find it in table
        //if not found, increase fault and push it into the table

        //number of upper bits required = log_2(2^64/pageSize) = 64-log_2(pageSize)
        
        // cout << acc.pid << "," << acc.va << "\n";
        // pageNum = acc.va >> (64-pageBits);
        pageNum = acc.va/pageSize;
        // cout << pageNum << "\n";
        if(pTable[acc.pid].findFrame(pageNum) == -1){
            // cout << "Page fault!!\n";
            faults++;
            pTable[acc.pid].enque(pageNum);
        }
        // pTable[acc.pid].printTable();
    }

    cout << "Number of page faults: " << faults << "\n";
}