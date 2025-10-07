#include <iostream>
#include <vector>
#include <utility>
#include "util.h"
#include <random>

namespace random_local{
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

        void evict(int frameNum){
            for(int i = 0; i < table.size(); i++){
                if(table[i].second == frameNum){
                    table.erase(table.begin()+i);
                    break;
                }
            }
        }
        
        void enque(uint64_t pageNum, vector<pageTable>& pTable){
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
                // Initialize a random number generator
                random_device rd;
                mt19937 gen(rd());
                uniform_int_distribution<> distrib(maxLength*pid, maxLength*(pid + 1)-1);

                // Generate random number in the range [min, max]
                int randomValue = distrib(gen);
                frameNum = physicalMemory[randomValue].frameNum;

                pTable[physicalMemory[randomValue].pid].evict(frameNum);
                physicalMemory[randomValue].pid = pid;
                physicalMemory[randomValue].pageNum = pageNum;
                physicalMemory[randomValue].status = 1;

                // frameNum = table[randomValue].second;  //frame number of the first allocated frame
                // table.erase(table.begin());
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

    int random_local(const vector<access>& trace, const int& pageSize, const int& numFrames){
        // cerr << 92;
        int faults = 0;
        // cerr << 93;
        frame* physicalMemory = new frame[numFrames];  //physical memory implemented simply as an array of frames
        for(int i = 0; i < numFrames; i++){
            physicalMemory[i].frameNum = i;
        }
        // cerr << 98;
        vector<pageTable> pTable;
        for(int i = 0; i < 4; i++){
            pTable.emplace_back(i, numFrames, physicalMemory);
        }
        // cerr << 103;
        uint64_t pageNum;

        for(access acc: trace){
            // cout << "\nNEW ACCESS STARTED\n" << endl;
            // cout << "ACCESSING PID:" << acc.pid << ":" << pageNum << endl;

            // cout << "Frame#\t\tpid\t\tpage\n";
            
            for(int i = 0; i < numFrames; i++){
                // cout << i << "\t\t" << physicalMemory[i].pid << "\t\t" << physicalMemory[i].pageNum << "\n";
            }

            pageNum = acc.va/pageSize;

            if(pTable[acc.pid].findFrame(pageNum) == -1){
                // cout << "Page fault!!\n";
                faults++;
                pTable[acc.pid].enque(pageNum, pTable);
            }
            // pTable[acc.pid].printTable();
        }
        // delete[] physicalMemory;
        return faults;
    }
}