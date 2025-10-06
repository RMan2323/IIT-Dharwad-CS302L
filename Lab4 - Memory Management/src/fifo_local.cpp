#include <iostream>
#include <vector>
#include <utility>
#include "util.h"

namespace fifo_local{
    class pageTable{
        vector<pair<uint64_t, int>> table;  //pageNum -> frameNum
        int maxLength;
        int pid;
        frame* physicalMemory;

        public:
        pageTable(int pid, int numFrames, frame* physicalMemory){
            //number of entries = 2^64/pageSize
            //maximum length requirement is 2^64/64 = 2^58
            //however, we simplify the simulation by making it similar to a inverted page table
            //maximum number of entries are numFrames, rest go to swapSpace
            this->pid = pid;
            maxLength = numFrames/4;
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
            for(int i = maxLength*pid; i < maxLength*pid + maxLength; i++){  //maxLength is same as numFrames/4
                if(physicalMemory[i].status == 0){
                    physicalMemory[i].status = 1;
                    physicalMemory[i].pid = pid;
                    physicalMemory[i].pageNum = pageNum;
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

    int fifo_local(const vector<access>& trace, const int& pageSize, const int& numFrames){
        int faults = 0;

        frame* physicalMemory = new frame[numFrames];  //physical memory implemented simply as an array of frames
        for(int i = 0; i < numFrames; i++){
            physicalMemory[i].frameNum = i;
        }

        vector<pageTable> pTable;
        for(int i = 0; i < 4; i++){
            pTable.emplace_back(i, numFrames, physicalMemory);
        }

        uint64_t pageNum;

        for(access acc: trace){
            // cout << "Frame#\t\tpid\t\tpage\n";
            // for(int i = 0; i < numFrames; i++){
            //     cout << i << "\t\t" << physicalMemory[i].pid << "\t\t" << physicalMemory[i].pageNum << "\n";
            // }

            pageNum = acc.va/pageSize;

            if(pTable[acc.pid].findFrame(pageNum) == -1){
                // cout << "Page fault!!\n";
                faults++;
                pTable[acc.pid].enque(pageNum);
            }
            // pTable[acc.pid].printTable();
        }
        delete[] physicalMemory;
        return faults;
    }
}