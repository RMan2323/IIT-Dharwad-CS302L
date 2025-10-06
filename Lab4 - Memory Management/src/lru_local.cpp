#include "util.h"

namespace lru_local{
    class physicalMemory{
        public:
        vector<frame> memoryVec;
        
        physicalMemory(int numFrames){
            memoryVec.reserve(numFrames);
            for(int i = 0; i < numFrames; i++){
                memoryVec.emplace_back(i);  //create numFrames-many frames
            }
        }
    };

    class pageTable{
        public:
        int pid;
        vector<pair<uint64_t, int>> table;  //pageNum -> frameNum
        int maxLength;
        vector<int> LRU; //the last frame will be MRU, first is LRU

        pageTable(int pid, int numFrames){
            this->pid = pid;
            maxLength = numFrames/4;
        }

        void printTable(){
            cout << "--- Table is: ---\n";
            for(const auto& entry: table){
                cout << entry.first << " -> " << entry.second << "\n";
            }
            cout << "--- End of table ---\n\n";
        }

        void printLRU(){
            for(int i = 0; i < LRU.size(); i++){
                cout << LRU[i] << " ";
            }
            cout << "\n";
        }

        int findFrame(uint64_t pageNum, physicalMemory& memory){
            // cout << "finding frames!\n";
            for(int i = 0; i < table.size(); i++){
                if(table[i].first == pageNum){
                    int frameNum = table[i].second;

                    //take out this frame and push it to the last
                    LRU.emplace_back(frameNum);
                    for(int j = 0; j < LRU.size()-1; j++){
                        if(LRU[j] == frameNum){
                            LRU.erase(LRU.begin()+j);
                            break;
                        }
                    }

                    return frameNum;
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
            for(int i = maxLength*pid; i < maxLength*pid + maxLength; i++){  //maxLength is same as numFrames/4
                if(memory.memoryVec[i].status == 0){
                    memory.memoryVec[i].status = 1;
                    memory.memoryVec[i].pid = pid;
                    memory.memoryVec[i].pageNum = pageNum;
                    
                    frameNum = memory.memoryVec[i].frameNum;

                    //take out this frame and push it to the last
                    LRU.emplace_back(frameNum);
                    for(int j = 0; j < LRU.size()-1; j++){
                        if(LRU[j] == frameNum){
                            LRU.erase(LRU.begin()+j);
                            break;
                        }
                    }

                    break;
                }
            }

            if(frameNum == -1){
                frameNum = LRU[0];

                pTable[memory.memoryVec[frameNum].pid].evict(frameNum);
                memory.memoryVec[frameNum].pid = pid;
                memory.memoryVec[frameNum].pageNum = pageNum;
                memory.memoryVec[frameNum].status = 1;

                //take out this frame and push it to the last
                LRU.emplace_back(frameNum);
                for(int j = 0; j < LRU.size()-1; j++){
                    if(LRU[j] == frameNum){
                        LRU.erase(LRU.begin()+j);
                        break;
                    }
                }
            }

            table.emplace_back(pageNum, frameNum);
        }
    };

    vector<pageTable> pTable;

    int lru_local(const vector<access>& trace, const int& pageSize, const int& numFrames){
        int faults = 0;

        physicalMemory memory(numFrames);

        pTable.reserve(4);
        for(int i = 0; i < 4; i++){
            pTable.emplace_back(i, numFrames);
        }

        uint64_t pageNum;

        for(access acc: trace){
            pageNum = acc.va/pageSize;
            
            // pTable[acc.pid].printLRU();

            int frameNum = pTable[acc.pid].findFrame(pageNum, memory);
            if(frameNum == -1){
                // cout << "Page fault\n";
                faults++;
                pTable[acc.pid].enque(pageNum, pTable, memory);
            }
        }

        return faults;
    }
}