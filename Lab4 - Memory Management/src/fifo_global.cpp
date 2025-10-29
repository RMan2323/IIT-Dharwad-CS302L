#include <iostream>
#include <vector>
#include <utility>
#include "util.h"

using namespace std;
namespace fifo_global{
    class physicalMemory{
        public:
        vector<frame> memoryVec;
        int head;  //for FIFO
        
        physicalMemory(int numFrames){
            memoryVec.reserve(numFrames);
            for(int i = 0; i < numFrames; i++){
                memoryVec.emplace_back(i);  //create numFrames-many frames
            }
            head = 0;
        }
    };

    class pageTable{
        public:
        int pid;
        vector<pair<uint64_t, int>> table;  //pageNum -> frameNum
        int maxLength;

        pageTable(int pid, int numFrames){
            this->pid = pid;
            maxLength = numFrames;
        }

        void printTable(){
            cout << "--- Table is: ---\n";
            for(const auto& entry: table){
                cout << entry.first << " -> " << entry.second << "\n";
            }
            cout << "--- End of table ---\n\n";
        }

        int findFrame(uint64_t pageNum){
            // cout << "finding frames!\n";
            for(int i = 0; i < table.size(); i++){
                if(table[i].first == pageNum){
                    return table[i].second;
                }
            }
            return -1;
        }

        void evict(int frameNum){
            for(int i = 0; i < table.size(); i++){
                if(table[i].second == frameNum){
                    table.erase(table.begin()+i);
                    break;
                }
            }
        }

        void enque(uint64_t pageNum, vector<pageTable>& pTable, physicalMemory& memory){
            int frameNum = -1;
            for(int i = 0; i < maxLength; i++){
                if(memory.memoryVec[i].status == 0){
                    memory.memoryVec[i].status = 1;
                    memory.memoryVec[i].pid = pid;
                    memory.memoryVec[i].pageNum = pageNum;
                    
                    frameNum = memory.memoryVec[i].frameNum;
                    break;
                }
            }

            if(frameNum == -1){
                frameNum = memory.memoryVec[memory.head].frameNum;

                pTable[memory.memoryVec[memory.head].pid].evict(frameNum);
                memory.memoryVec[memory.head].pid = pid;
                memory.memoryVec[memory.head].pageNum = pageNum;
                memory.memoryVec[memory.head].status = 1;
                memory.head = ((memory.head)+1)%maxLength;
            }

            table.emplace_back(pageNum, frameNum);
        }
    };

    vector<pageTable> pTable;

    int fifo_global(const vector<access>& trace, const int& pageSize, const int& numFrames){
        int faults = 0;

        physicalMemory memory(numFrames);

        pTable.reserve(4);
        for(int i = 0; i < 4; i++){
            pTable.emplace_back(i, numFrames);
        }

        uint64_t pageNum;

        vector<int> perProcessFaults(4, 0); // Vector to store fault counts for each process

        for(access acc: trace){
            pageNum = acc.va/pageSize;
            
            int frameNum = pTable[acc.pid].findFrame(pageNum);
            if(frameNum == -1){
                // cout << "Page fault\n";
                faults++;
                perProcessFaults[acc.pid]++; // Increment fault count for the specific process
                pTable[acc.pid].enque(pageNum, pTable, memory);
            }
        }

        // Print the per-process fault counts
        // cout << "    FIFO Global Per-Process Faults:\n";
        for (int i = 0; i < 4; ++i) {
            cout << "      P" << i << ": " << perProcessFaults[i] << " \n";
        }

        return faults;
    }
}