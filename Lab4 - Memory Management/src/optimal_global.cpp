#include "util.h"
#include <vector>
#include <iostream>

namespace optimal_global {

    class physicalMemory {
    public:
        vector<frame> memoryVec;

        physicalMemory(int numFrames) {
            memoryVec.reserve(numFrames);
            for (int i = 0; i < numFrames; i++) {
                memoryVec.emplace_back(i); // Create numFrames-many frames
            }
        }
    };

    class pageTable {
    public:
        int pid;
        vector<pair<uint64_t, int>> table; // pageNum -> frameNum
        int maxLength;

        pageTable(int pid, int numFrames) {
            this->pid = pid;
            this->maxLength = numFrames;
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

        void enque(uint64_t pageNum, vector<pageTable>& pTable, physicalMemory& memory,
                   int current_trace_idx, const vector<access>& trace, int pageSize) {
            
            // 1. Find a free frame first
            int frameNum = -1;
            for (int i = 0; i < maxLength; i++) {
                if (memory.memoryVec[i].status == 0) {
                    frameNum = i;
                    break;
                }
            }

            // 2. If no free frame, find the optimal page to evict
            if (frameNum == -1) {
                int victimFrame = -1;
                int furthestNextUse = -1;

                // Iterate through all frames in physical memory
                for (int i = 0; i < maxLength; ++i) {
                    uint64_t currentPageNum = memory.memoryVec[i].pageNum;
                    int currentPid = memory.memoryVec[i].pid;
                    int nextUse = -1;

                    // Look ahead in the trace to find the next use of this page
                    for (int j = current_trace_idx + 1; j < trace.size(); ++j) {
                        if (trace[j].pid == currentPid && (trace[j].va / pageSize) == currentPageNum) {
                            nextUse = j;
                            break;
                        }
                    }

                    // If a page is never used again, it's the best one to evict
                    if (nextUse == -1) {
                        victimFrame = i;
                        break;
                    }

                    // Otherwise, track the one that will be used furthest in the future
                    if (nextUse > furthestNextUse) {
                        furthestNextUse = nextUse;
                        victimFrame = i;
                    }
                }
                
                frameNum = victimFrame;
                // Evict the victim page from its process's page table
                pTable[memory.memoryVec[frameNum].pid].evict(frameNum);
            }

            // 3. Place the new page into the chosen frame
            memory.memoryVec[frameNum].status = 1;
            memory.memoryVec[frameNum].pid = pid;
            memory.memoryVec[frameNum].pageNum = pageNum;
            table.emplace_back(pageNum, frameNum);
        }
    };

    int optimal_global(const vector<access>& trace, const int& pageSize, const int& numFrames) {
        int faults = 0;
        physicalMemory memory(numFrames);
        vector<pageTable> pTable;
        pTable.reserve(4);
        for (int i = 0; i < 4; i++) {
            pTable.emplace_back(i, numFrames);
        }

        vector<int> perProcessFaults(4, 0); // Vector to store fault counts for each process

        for (int i = 0; i < trace.size(); ++i) {
            const auto& acc = trace[i];
            uint64_t pageNum = acc.va / pageSize;
            
            int frameNum = pTable[acc.pid].findFrame(pageNum);
            if (frameNum == -1) {
                faults++;
                perProcessFaults[acc.pid]++; // Increment fault count for the specific process
                pTable[acc.pid].enque(pageNum, pTable, memory, i, trace, pageSize);
            }
        }

        // Print the per-process fault counts
        cout << "    Optimal Global Per-Process Faults:\n";
        for (int i = 0; i < 4; ++i) {
            cout << "      Process " << i << ": " << perProcessFaults[i] << " faults\n";
        }

        return faults;
    }
}