#include <iostream>
#include <vector>
#include <utility>
#include "util.h"
#include <random>

using namespace std;
namespace random_global{
    class physicalMemory{
        public:
        vector<frame> memoryVec;
        
        physicalMemory(int numFrames){
            memoryVec.reserve(numFrames);
            for(int i = 0; i < numFrames; i++){
                memoryVec.emplace_back(i);  //create numFrames-many frames
            }
            // head = 0;
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

        void enque(uint64_t pageNum, vector<pageTable>& pTable, physicalMemory& memory, int numFrames){
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
                // Initialize a random number generator
                random_device rd;
                mt19937 gen(rd());
                uniform_int_distribution<> distrib(0, numFrames-1);

                // Generate random number in the range [min, max]
                int randomValue = distrib(gen);
                frameNum = memory.memoryVec[randomValue].frameNum;

                pTable[memory.memoryVec[randomValue].pid].evict(frameNum);
                memory.memoryVec[randomValue].pid = pid;
                memory.memoryVec[randomValue].pageNum = pageNum;
                memory.memoryVec[randomValue].status = 1;
            }

            table.emplace_back(pageNum, frameNum);
        }
    };

    vector<pageTable> pTable;

    int random_global(const vector<access>& trace, const int& pageSize, const int& numFrames){
        int faults = 0;

        physicalMemory memory(numFrames);

        pTable.reserve(4);
        for(int i = 0; i < 4; i++){
            pTable.emplace_back(i, numFrames);
        }

        uint64_t pageNum;

        for(access acc: trace){
            // cout << "\nNEW ACCESS STARTED\n" << endl;
            // pTable[acc.pid].printTable();

            pageNum = acc.va/pageSize;
            
            // cout << "ACCESSING PID:" << acc.pid << ":" << pageNum << endl;

            int frameNum = pTable[acc.pid].findFrame(pageNum);
            if(frameNum == -1){
                // cout << "Page fault\n";
                faults++;
                pTable[acc.pid].enque(pageNum, pTable, memory, numFrames);
            }
            // pTable[acc.pid].printTable();
        }

        return faults;
    }
}