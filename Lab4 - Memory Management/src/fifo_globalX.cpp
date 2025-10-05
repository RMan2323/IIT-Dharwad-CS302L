#include <iostream>
#include <vector>
#include <queue>
#include <utility>
#include <math.h>
#include "util.h"

class physicalMemory{
    public:
    vector<frame> memoryVec;
    int head;  //head of queue for FIFO behaviour

    physicalMemory(){
        head = 0;
    }
};

class pageTable{
    public:
    int pid;
    vector<pair<uint64_t, int>> table;  //pageNum -> frameNum
    int maxLength;
    physicalMemory* memory;

    pageTable(int pid, int numFrames, physicalMemory& memory){
        //number of entries = 2^64/pageSize
        //maximum length requirement is 2^64/64 = 2^58
        //however, we simplify the simulation by making it similar to a inverted page table
        //maximum number of entries are numFrames, rest go to swapSpace
        this->pid = pid;
        maxLength = numFrames;
        this->memory = &memory;
    }

    void printTable(){
    cout << "table.size() = " << table.size() << endl;
    cout << "--- Table is: ---\n";
    for(const auto& entry : table){
        cout << entry.first << " -> " << entry.second << "\n";
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
        for(int i = 0; i < maxLength; i++){  //maxLength is same as numFrames
            if(memory->memoryVec[i].status == 0){
                memory->memoryVec[i].status = 1;
                memory->memoryVec[i].pid = pid;
                memory->memoryVec[i].pageNum = pageNum;
                
                frameNum = memory->memoryVec[i].frameNum;
                break;
            }
            // if(memory.memory->at(i).status == 0){
            //     physicalMemory->at(i).status = 1;
            //     physicalMemory->at(i).pid = pid;
            //     physicalMemory->at(i).pageNum = pageNum;
            //     frameNum = physicalMemory->at(i).frameNum;
            //     break;
            // }
        }

        //if no free frame found, evict page at physicalMemory[0] and allocate its frame
        if(frameNum == -1){
            frameNum = memory->memoryVec[memory->head].frameNum;
            pTable[memory->memoryVec[memory->head].pid].evict(memory->memoryVec[memory->head].frameNum);  //evict page of pid currently at frameNum
            memory->head = ((memory->head)+1)%maxLength;

            // frameNum = physicalMemory->at(0).frameNum;
            // pTable[physicalMemory->at(0).pid].evict(physicalMemory->at(0).frameNum);  //evict page of pid currently at frameNum
            // physicalMemory->erase(physicalMemory->begin());  //remove frame from front of the queue
            // physicalMemory->emplace_back(frameNum, pid, pageNum);  //push the frame again
        }

        table.emplace_back(pageNum, frameNum);
        cout << "Emplaced " << pageNum << " to " << frameNum << "\n";

        auto& last = table.back();
        cout << "Inserted: pageNum = " << last.first << ", frameNum = " << last.second << "\n";

    }

    //returns frameNum if found, else -1;
    int findFrame(uint64_t pageNum){
        cerr << "finding!!!!!!!!!!!!!!!!!!\n" << std::flush;
        for(int i = 0; i < table.size(); i++){
            if(table[i].first == pageNum){
                return table[i].second;
            }
        }
        return -1;
    }
};

void fifo_globalX(const vector<access>& trace, const int& pageSize, const int& numFrames){
    // cout << numFrames;
    int faults = 0;
    // cout << 76;
    physicalMemory memory;
    for(int i = 0; i < numFrames; i++){
        memory.memoryVec.emplace_back(i);  //frameNum, pid, pageNum
    }
    // cout << 81;
    vector<pageTable> pTable;
    pTable.reserve(4);
    for(int i = 0; i < 4; i++){
        pTable.emplace_back(i, numFrames, memory);
        // cout<<85;
    }
    // cout << 86;
    // int pageBits = 64-log2(pageSize);
    uint64_t pageNum;
    // cout << "89\n";
    for(access acc: trace){
        for(int i = 0; i < numFrames; i++)  //maxLength is same as numFrames
            cout << "Frame: " << i << ", pid: " << memory.memoryVec[i].pid << ", page: " << memory.memoryVec[i].pageNum << ", status: " << memory.memoryVec[i].status << "\n";
        
        //get the page number
        //find it in table
        //if not found, increase fault and push it into the table

        //number of upper bits required = log_2(2^64/pageSize) = 64-log_2(pageSize)
        
        cout << acc.pid << "," << acc.va << "\n";
        // pageNum = acc.va >> (64-pageBits);
        pageNum = acc.va/pageSize;
        cout << "[DEBUG] Before findFrame, pageNum = " << pageNum << endl;
        int result = pTable[acc.pid].findFrame(pageNum);
        cout << "[DEBUG] After findFrame, result = " << result << endl;
        if(result == -1){
            cout << "Page fault!!\n";
            faults++;
            pTable[acc.pid].enque(pageNum, pTable);
            cout << "Enqued\n";
        }
        for(int i = 0; i < numFrames; i++)  //maxLength is same as numFrames
            cout << "Frame: " << i << ", pid: " << memory.memoryVec[i].pid << ", page: " << memory.memoryVec[i].pageNum << ", status: " << memory.memoryVec[i].status << "\n";
        // cout << "Accesing: " << acc.pid << "\n";
        pTable[acc.pid].printTable();
    }

    cout << "Number of page faults: " << faults;
}