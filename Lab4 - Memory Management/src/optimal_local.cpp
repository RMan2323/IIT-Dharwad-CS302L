#include "util.h"
#include <vector>
#include <iostream>

namespace optimal_local {

    class pageTable {
    private:
        std::vector<std::pair<uint64_t, int>> table; // pageNum -> frameNum
        int maxLength;
        int pid;
        frame* physicalMemory;

    public:
        pageTable(int pid, int numFrames, frame* physicalMemory) {
            this->pid = pid;
            this->maxLength = numFrames / 4;
            this->physicalMemory = physicalMemory;
        }

        int findFrame(uint64_t pageNum) {
            for (const auto& entry : table) {
                if (entry.first == pageNum) {
                    return entry.second;
                }
            }
            return -1;
        }
        
        void evict(int frameNum) {
            for (auto it = table.begin(); it != table.end(); ++it) {
                if (it->second == frameNum) {
                    table.erase(it);
                    break;
                }
            }
        }

        void enque(uint64_t pageNum, int current_trace_idx, const std::vector<access>& trace, int pageSize) {
            int frameNum = -1;
            int frameStart = maxLength * pid;
            int frameEnd = frameStart + maxLength;

            // 1. Find a free frame in this process's partition
            for (int i = frameStart; i < frameEnd; i++) {
                if (physicalMemory[i].status == 0) {
                    frameNum = i;
                    break;
                }
            }
            
            // 2. If no free frame, find the optimal page to evict from this process's frames
            if (frameNum == -1) {
                int victimFrame = -1;
                int furthestNextUse = -1;
                
                // Iterate through the pages currently held by this process
                for(const auto& entry : table){
                    uint64_t currentPageNum = entry.first;
                    int nextUse = -1;

                    // Look ahead in the trace for its next use
                    for (int j = current_trace_idx + 1; j < trace.size(); ++j) {
                        if (trace[j].pid == pid && (trace[j].va / pageSize) == currentPageNum) {
                            nextUse = j;
                            break;
                        }
                    }

                    if (nextUse == -1) {
                        victimFrame = entry.second;
                        break;
                    }

                    if (nextUse > furthestNextUse) {
                        furthestNextUse = nextUse;
                        victimFrame = entry.second;
                    }
                }
                
                frameNum = victimFrame;
                // Evict the victim page from this table (no need to check other tables)
                evict(frameNum);
            }

            // 3. Place the new page into the chosen frame
            physicalMemory[frameNum].status = 1;
            physicalMemory[frameNum].pid = pid;
            physicalMemory[frameNum].pageNum = pageNum;
            table.emplace_back(pageNum, frameNum);
        }
    };

    int optimal_local(const std::vector<access>& trace, const int& pageSize, const int& numFrames) {
        int faults = 0;
        frame* physicalMemory = new frame[numFrames];
        for (int i = 0; i < numFrames; i++) {
            physicalMemory[i].frameNum = i;
        }

        std::vector<pageTable> pTable;
        for (int i = 0; i < 4; i++) {
            pTable.emplace_back(i, numFrames, physicalMemory);
        }

        for (int i = 0; i < trace.size(); ++i) {
            const auto& acc = trace[i];
            uint64_t pageNum = acc.va / pageSize;

            if (pTable[acc.pid].findFrame(pageNum) == -1) {
                faults++;
                pTable[acc.pid].enque(pageNum, i, trace, pageSize);
            }
        }
        
        delete[] physicalMemory;
        return faults;
    }
}