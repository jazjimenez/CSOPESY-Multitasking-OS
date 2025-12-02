#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <mutex>
#include <map>
#include <cstdint>

// added libraries
#include <algorithm>
#include <queue>
#include <list>
#include <sstream>
#include <cmath>
#include <atomic>
#include <set>

void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

std::string ascii_art = R"(
  ____ ____   ____  _____  _______ _________   __
 / ___/ ___| / _  \|  _  \|  _____/  ____\  \ / /
| |   \___ \| | |  | |_)  |   __| \____  \\  V /
| |___ ___) | |_|  |  ___/|  |____ ____)  ||  |
 \____|____/ \____/|_|    |_______|______/ |__|

--------------------------------------------------
)";
std::string header = "Welcome to CSOPESY Emulator!\n\nGroup Developers:\nCastillo, Marvien Angel\nHerrera, Mikaela Gabrielle\nJimenez, Jaztin Jacob\nRegindin, Sean Adrien\n\nLast Updated: 12-02-2025\n";

using namespace std;

typedef struct {
    int numCPU;
    string schedulingAlgorithm;
    int timeQuantum;
    int batchFreq;
    int minCommand;
    int maxCommand;
    int delayTime;
    // Memory configuration
    size_t maxOverallMem; // total size memory
    size_t memPerFrame; // size of each memory
    size_t minMemPerProc; // minimum mem per
    size_t maxMemPerProc; // maximum mem per process
} Config;

bool is_initialized = false;
Config config;

enum InstructionType {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR_LOOP,
    READ,  // The new instruction stuff
    WRITE  // The new instruction stuff
};

class Instruction {
public:
    InstructionType type;
    vector<string> params;
    
    Instruction(InstructionType t, vector<string> p) : type(t), params(p) {}
    
    string toString() {
        string result;
        switch(type) {
            case PRINT: 
                result = "PRINT(" + params[0] + ")";
                break;
            case DECLARE:
                result = "DECLARE(" + params[0] + ", " + params[1] + ")";
                break;
            case ADD:
                result = "ADD(" + params[0] + ", " + params[1] + ", " + params[2] + ")";
                break;
            case SUBTRACT:
                result = "SUBTRACT(" + params[0] + ", " + params[1] + ", " + params[2] + ")";
                break;
            case SLEEP:
                result = "SLEEP(" + params[0] + ")";
                break;
            case FOR_LOOP:
                result = "FOR(...)";
                break;
            // New stuff
            case READ:
                result = "READ " + params[0] + " " + params[1];
                break;
            case WRITE:
                result = "WRITE " + params[0] + " " + params[1];
                break;
        }
        return result;
    }
};

// New stuff
struct MemoryFrame {
    size_t frameIndex; // index of the frame in the frame table
    bool isOccupied; // whether the frame is occupied
    int processID; // id process occupying the frame, -1 if free
    
    MemoryFrame(size_t idx) : frameIndex(idx), isOccupied(false), processID(-1) {}
};


// check if memory size is power of 2 and within range
bool isValidMemorySize(size_t memSize) {
    // Must be power of 2 and within range [64, 65536]
    if (memSize < 64 || memSize > 65536) return false;
    return (memSize & (memSize - 1)) == 0;
}

class Process {
public:
    string name;
    int pID;
    int coreAssigned;
    int totalInstruction;
    int currentInstruction;
    bool isFinished;
    // memory error tracking
    bool hasMemoryError;
    string memoryErrorAddress;

    time_t startTime;
    time_t endTime;
    time_t errorTime;
    
    // memory information (adjusted for demand paging)
    size_t memoryRequired;
    bool memoryAllocated;

    // damand paging stuff
    set<size_t> pagesInMemory; // the pages currently in physical memory
    map<size_t, size_t> pageTable; // page number to frame index
    atomic<int> pageFaultCount{0}; // number of page faults

    // symbol table
    map<string, uint16_t> symbolTable;
    static const size_t SYMBOL_TABLE_SIZE = 64;
    static const size_t MAX_VARIABLES = 32;

    // process memory (variable storage)
    map<size_t, uint16_t> processMemory;

    vector<Instruction> instructions;
    vector<string> outputLog;

    atomic<int> consecutivePageFaults{0};  // track consecutive failures


    bool canFitInMemory() const {
        return memoryRequired <= config.maxOverallMem;
    }

    // calculate threshold based on system configuration
    int getMaxConsecutiveFaults() const {
        // If this process cannot physically fit in memory, fail fast
        if (!canFitInMemory()) {
            return 5; 
        }
        
        // If process can fit but memory is fragmented/contested
        size_t totalFrames = config.maxOverallMem / config.memPerFrame;
        return max(50, (int)(totalFrames * 2));
    }
    
    bool isDeadlocked() const {
        return consecutivePageFaults.load() >= getMaxConsecutiveFaults();
    }



private:
    void generateInstructions(int count) {
        for (int i = 0; i < count; ++i) {
            int instrType = rand() % 6;
            switch (instrType) {
                case 0:
                    instructions.emplace_back(PRINT, vector<string>{"\"Hello world from " + name + "!\""});
                    break;
                case 1:
                    instructions.emplace_back(DECLARE, vector<string>{"var" + to_string(i), to_string(rand() % 100)});
                    break;
                case 2: {
                    int prevIdx = (i > 0) ? (i - 1) : 0;
                    instructions.emplace_back(ADD, vector<string>{
                        "var" + to_string(i), 
                        "var" + to_string(prevIdx), 
                        to_string(rand() % 50)
                    });
                    break;
                }
                case 3: {
                    int prevIdx = (i > 0) ? (i - 1) : 0;
                    instructions.emplace_back(SUBTRACT, vector<string>{
                        "var" + to_string(i), 
                        "var" + to_string(prevIdx), 
                        to_string(rand() % 30)
                    });
                    break;
                }
                case 4:
                    instructions.emplace_back(SLEEP, vector<string>{to_string(rand() % 5 + 1)});
                    break;
                case 5:
                    instructions.emplace_back(FOR_LOOP, vector<string>{to_string(rand() % 3 + 2)});
                    break;
            }
        }
    }

    bool isValidMemoryAddress(size_t address) {
        // address must be within allocated memory and not in symbol table
        return address >= SYMBOL_TABLE_SIZE && address < memoryRequired;
    }

public:
    // demand paging stuff again
    size_t getPageNumber(size_t address) {
        return address / config.memPerFrame;
    }

    bool isPageInMemory(size_t pageNumber) {
        return pagesInMemory.find(pageNumber) != pagesInMemory.end();
    }

    size_t getTotalPages() const {
        return (memoryRequired + config.memPerFrame - 1) / config.memPerFrame;
    }

    void addPageToMemory(size_t pageNumber, size_t frameIndex) {
        pagesInMemory.insert(pageNumber);
        pageTable[pageNumber] = frameIndex;
    }

    void removePageFromMemory(size_t pageNumber) {
        pagesInMemory.erase(pageNumber);
        pageTable.erase(pageNumber);
    }


    Process(int pID, string name, int maxIns, int minIns, size_t minMem, size_t maxMem) {
        this->pID = pID;
        this->name = name;
        this->coreAssigned = -1;
        this->totalInstruction = rand() % (maxIns - minIns + 1) + minIns;
        this->currentInstruction = 0;
        isFinished = false;
        hasMemoryError = false;
        this->startTime = time(nullptr);
        
        // NEW: Random memory requirement
        this->memoryRequired = (rand() % (maxMem - minMem + 1) + minMem);
        if (this->memoryRequired < SYMBOL_TABLE_SIZE) this->memoryRequired = SYMBOL_TABLE_SIZE;
        this->memoryAllocated = false;
        
        generateInstructions(this->totalInstruction);
    }

    // constructor for screen -c (user-created process)
    Process(int pID, string name, size_t memSize, const vector<Instruction>& userInstructions) {
        this->pID = pID;
        this->name = name;
        this->coreAssigned = -1;
        this->totalInstruction = userInstructions.size();
        this->currentInstruction = 0;
        isFinished = false;
        hasMemoryError = false;
        this->startTime = time(nullptr);
        
        this->memoryRequired = memSize;
        if (this->memoryRequired < SYMBOL_TABLE_SIZE) this->memoryRequired = SYMBOL_TABLE_SIZE;
        this->memoryAllocated = false;
        
        this->instructions = userInstructions;
    }

    void executeInstruction(int index) {
        if (index >= instructions.size() || hasMemoryError) return;
        
        Instruction& inst = instructions[index];
        
        switch(inst.type) {
            case PRINT: {
                string msg = inst.params[0];
                if (msg.front() == '"' && msg.back() == '"') {
                    msg = msg.substr(1, msg.length() - 2);
                }
                
                // Handle variable concatenation
                size_t pos = 0;
                while ((pos = msg.find(" + ")) != string::npos) {
                    string before = msg.substr(0, pos);
                    string after = msg.substr(pos + 3);
                    
                    // Check if after is a variable
                    if (symbolTable.find(after) != symbolTable.end()) {
                        msg = before + to_string(symbolTable[after]);
                    } else {
                        msg = before + after;
                    }
                }
                
                time_t now = time(nullptr);
                struct tm timeinfo;
                localtime_s(&timeinfo, &now);
                char timestamp[32];
                strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", &timeinfo);
                
                string logEntry = "(" + string(timestamp) + ") Core:" 
                                + to_string(coreAssigned) + " \"" + msg + "\"";
                outputLog.push_back(logEntry);
                break;
            }
            
            case DECLARE: {
                if (symbolTable.size() >= MAX_VARIABLES) {
                    break;
                }
                
                string var = inst.params[0];
                uint16_t value = 0;
                try {
                    value = (uint16_t)stoi(inst.params[1]);
                } catch (...) {
                    value = 0;
                }
                symbolTable[var] = value;
                break;
            }
            
            case ADD: {
                string var1 = inst.params[0];
                if (symbolTable.size() >= MAX_VARIABLES && symbolTable.find(var1) == symbolTable.end()) {
                    break;
                }
                if (symbolTable.find(var1) == symbolTable.end()) {
                    symbolTable[var1] = 0;
                }
                
                uint16_t val2 = 0;
                if (symbolTable.find(inst.params[1]) != symbolTable.end()) {
                    val2 = symbolTable[inst.params[1]];
                } else {
                    try {
                        val2 = (uint16_t)stoi(inst.params[1]);
                    } catch (...) {
                        val2 = 0;
                    }
                }
                
                uint16_t val3 = 0;
                if (symbolTable.find(inst.params[2]) != symbolTable.end()) {
                    val3 = symbolTable[inst.params[2]];
                } else {
                    try {
                        val3 = (uint16_t)stoi(inst.params[2]);
                    } catch (...) {
                        val3 = 0;
                    }
                }
                
                uint32_t result = (uint32_t)val2 + (uint32_t)val3;
                if (result > UINT16_MAX) result = UINT16_MAX;
                
                symbolTable[var1] = (uint16_t)result;
                break;
            }
            
            case SUBTRACT: {
                string var1 = inst.params[0];
                if (symbolTable.size() >= MAX_VARIABLES && symbolTable.find(var1) == symbolTable.end()) {
                    break;
                }
                if (symbolTable.find(var1) == symbolTable.end()) {
                    symbolTable[var1] = 0;
                }
                
                uint16_t val2 = 0;
                if (symbolTable.find(inst.params[1]) != symbolTable.end()) {
                    val2 = symbolTable[inst.params[1]];
                } else {
                    try {
                        val2 = (uint16_t)stoi(inst.params[1]);
                    } catch (...) {
                        val2 = 0;
                    }
                }
                
                uint16_t val3 = 0;
                if (symbolTable.find(inst.params[2]) != symbolTable.end()) {
                    val3 = symbolTable[inst.params[2]];
                } else {
                    try {
                        val3 = (uint16_t)stoi(inst.params[2]);
                    } catch (...) {
                        val3 = 0;
                    }
                }
                
                int32_t result = (int32_t)val2 - (int32_t)val3;
                if (result < 0) result = 0;
                
                symbolTable[var1] = (uint16_t)result;
                break;
            }
            
            case READ: {
                string varName = inst.params[0];
                string addressStr = inst.params[1];
                
                if (symbolTable.size() >= MAX_VARIABLES && symbolTable.find(varName) == symbolTable.end()) {
                    break;
                }
                
                // Parse hex address
                size_t address;
                try {
                    address = stoull(addressStr, nullptr, 16);
                } catch (...) {
                    hasMemoryError = true;
                    memoryErrorAddress = addressStr;
                    errorTime = time(nullptr);
                    isFinished = true;
                    break;
                }
                
                if (!isValidMemoryAddress(address)) {
                    hasMemoryError = true;
                    memoryErrorAddress = addressStr;
                    errorTime = time(nullptr);
                    isFinished = true;
                    break;
                }
                
                uint16_t value = (processMemory.find(address) != processMemory.end()) 
                                ? processMemory[address] : 0;
                symbolTable[varName] = value;
                break;
            }
            
            case WRITE: {
                string addressStr = inst.params[0];
                uint16_t value = 0;
                
                if (symbolTable.find(inst.params[1]) != symbolTable.end()) {
                    value = symbolTable[inst.params[1]];
                } else {
                    try {
                        value = (uint16_t)stoi(inst.params[1]);
                    } catch (...) {
                        value = 0;
                    }
                }
                
                size_t address;
                try {
                    address = stoull(addressStr, nullptr, 16);
                } catch (...) {
                    hasMemoryError = true;
                    memoryErrorAddress = addressStr;
                    errorTime = time(nullptr);
                    isFinished = true;
                    break;
                }
                
                if (!isValidMemoryAddress(address)) {
                    hasMemoryError = true;
                    memoryErrorAddress = addressStr;
                    errorTime = time(nullptr);
                    isFinished = true;
                    break;
                }
                
                processMemory[address] = value;
                break;
            }
            
            case SLEEP:
            case FOR_LOOP:
                break;
        }
    }
};

class MemoryManager {
private:
    vector<MemoryFrame> frames; // frame table
    size_t totalFrames;         // total number of frames
    size_t frameSize;           // size of each frame
    size_t totalMemory;         // total memory size
    mutex memMutex;         // mutex for thread safety
    mutex backingStoreMutex;    // mutex for backing store file access

    // for demand paging again
    queue<pair<int, size_t>> fifoQueue; // FIFO queue for page replacement
    // logs stuff to csopesy-backing-store.txt
    void logToBackingStore(const string& message) {
        lock_guard<mutex> lock(backingStoreMutex);
        ofstream backingStore("csopesy-backing-store.txt", ios::out | ios::app);
        if (backingStore.is_open()) {
            time_t now = time(nullptr);
            struct tm timeinfo;
            localtime_s(&timeinfo, &now);
            char timestamp[32];
            strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", &timeinfo);
            
            backingStore << timestamp << " " << message << "\n";
            backingStore.close();
        }
    }
    
public:
    // initialize memory manager
    MemoryManager(size_t maxMem, size_t memPerFrame) 
        : totalMemory(maxMem), frameSize(memPerFrame) {
        totalFrames = maxMem / memPerFrame;

        // create all the frames
        for (size_t i = 0; i < totalFrames; i++) {
            frames.push_back(MemoryFrame(i));
        }
        
        // Initialize backing store file
        ofstream backingStore("csopesy-backing-store.txt", ios::out | ios::trunc);
        if (backingStore.is_open()) {
            backingStore << "========================================\n";
            backingStore << "CSOPESY BACKING STORE\n";
            backingStore << "========================================\n";
            backingStore << "Total Memory: " << totalMemory << " bytes\n";
            backingStore << "Frame Size: " << frameSize << " bytes\n";
            backingStore << "Total Frames: " << totalFrames << "\n";
            backingStore << "========================================\n\n";
            backingStore.close();
        }
    }
    
    // changed to demand paging instead of just allocating memory
    pair<bool, size_t> allocatePage(int processID, size_t pageNum) {
        lock_guard<mutex> lock(memMutex);
        
        // Find first free frame
        for (size_t i = 0; i < frames.size(); i++) {
            if (!frames[i].isOccupied) {
                frames[i].isOccupied = true;
                frames[i].processID = processID;
                
                // Add to FIFO queue for replacement
                fifoQueue.push({processID, pageNum});
                
                stringstream ss;
                ss << "PAGE IN: Process " << processID 
                   << " - Page " << pageNum 
                   << " loaded into Frame " << i;
                logToBackingStore(ss.str());
                
                return {true, i};  // Return frame index
            }
        }
        
        return {false, 0};  // No free frames available
    }

    // check if memory is completely full
    bool isFull() {
        lock_guard<mutex> lock(memMutex);
        for (const auto& frame : frames) {
            if (!frame.isOccupied) return false;
        }
        return true;
    }
    
    // Dselect victim page for replacement (FIFO)
    pair<int, size_t> selectVictimPage() {
        lock_guard<mutex> lock(memMutex);
        
        if (fifoQueue.empty()) {
            return {-1, 0};
        }
        
        auto victim = fifoQueue.front();
        fifoQueue.pop();
        return victim;
    }
    
    // evict a specific page from memory
    bool evictPage(int processID, size_t pageNum, Process* process) {
        lock_guard<mutex> lock(memMutex);
        
        // find the frame containing this page
        for (auto& frame : frames) {
            if (frame.processID == processID && frame.isOccupied) {
                // check if this frame contains the target page
                // we need to check the process's page table
                if (process != nullptr && process->pageTable.find(pageNum) != process->pageTable.end()) {
                    size_t frameIdx = process->pageTable[pageNum];
                    if (frameIdx == frame.frameIndex) {
                        // found the correct frame
                        frame.isOccupied = false;
                        frame.processID = -1;
                        
                        stringstream ss;
                        ss << "PAGE OUT: Process " << processID 
                           << " - Page " << pageNum 
                           << " evicted from Frame " << frame.frameIndex;
                        logToBackingStore(ss.str());
                        
                        // remove from process's page table
                        process->removePageFromMemory(pageNum);
                        
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    // deallocate the pages after process is finished
    void deallocateMemory(int processID) {
        lock_guard<mutex> lock(memMutex);
        
        int deallocated = 0;
        vector<size_t> freedFrames;
        
        for (auto& frame : frames) {
            if (frame.processID == processID) {
                freedFrames.push_back(frame.frameIndex);
                frame.isOccupied = false;
                frame.processID = -1;
                deallocated++;
            }
        }
        
        // Remove from FIFO queue
        queue<pair<int, size_t>> newQueue;
        while (!fifoQueue.empty()) {
            auto item = fifoQueue.front();
            fifoQueue.pop();
            if (item.first != processID) {
                newQueue.push(item);
            }
        }
        fifoQueue = newQueue;
        
        if (deallocated > 0) {
            stringstream ss;
            ss << "DEALLOCATE ALL: Process " << processID 
               << " - " << deallocated << " pages freed - Frames: [";
            for (size_t i = 0; i < freedFrames.size(); i++) {
                ss << freedFrames[i];
                if (i < freedFrames.size() - 1) ss << ", ";
            }
            ss << "]";
            logToBackingStore(ss.str());
        }
    }
    
    // get total used memory for vmstat
    size_t getUsedMemory() {
        lock_guard<mutex> lock(memMutex);
        size_t used = 0;
        for (const auto& frame : frames) {
            if (frame.isOccupied) used++;
        }
        return used * frameSize;
    }
    
    // get total free memory for vmstat
    size_t getFreeMemory() {
        return totalMemory - getUsedMemory();
    }
    
    // getters
    size_t getTotalFrames() { return totalFrames; }
    size_t getFrameSize() { return frameSize; }
    
    // additional stats
    int getNumPagesUsed() {
        lock_guard<mutex> lock(memMutex);
        int count = 0;
        for (const auto& frame : frames) {
            if (frame.isOccupied) count++;
        }
        return count;
    }
    
    // get number of free pages
    int getNumPagesFree() {
        return totalFrames - getNumPagesUsed();
    }

    // get memory usage per process
    map<int, int> getProcessMemoryUsage() {
        lock_guard<mutex> lock(memMutex);
        map<int, int> processFrames;
        
        for (const auto& frame : frames) {
            if (frame.isOccupied) {
                processFrames[frame.processID]++;
            }
        }
        
        return processFrames;
    }
};
void printNotInitialized() {
    cout << "Please initialize the system first!" << endl;
}

// Parser for user-defined instructions
vector<Instruction> parseInstructions(const string& instructionStr) {
    vector<Instruction> instructions;
    stringstream ss(instructionStr);
    string instrLine;
    
    while (getline(ss, instrLine, ';')) {
        // Trim whitespace
        instrLine.erase(0, instrLine.find_first_not_of(" \t\n\r"));
        instrLine.erase(instrLine.find_last_not_of(" \t\n\r") + 1);
        
        if (instrLine.empty()) continue;
        
        stringstream instrStream(instrLine);
        string command;
        instrStream >> command;
        
        if (command == "DECLARE") {
            string var, value;
            instrStream >> var >> value;
            instructions.emplace_back(DECLARE, vector<string>{var, value});
        }
        else if (command == "ADD") {
            string var1, var2, var3;
            instrStream >> var1 >> var2 >> var3;
            instructions.emplace_back(ADD, vector<string>{var1, var2, var3});
        }
        else if (command == "SUBTRACT") {
            string var1, var2, var3;
            instrStream >> var1 >> var2 >> var3;
            instructions.emplace_back(SUBTRACT, vector<string>{var1, var2, var3});
        }
        else if (command == "READ") {
            string var, addr;
            instrStream >> var >> addr;
            instructions.emplace_back(READ, vector<string>{var, addr});
        }
        else if (command == "WRITE") {
            string addr, value;
            instrStream >> addr >> value;
            instructions.emplace_back(WRITE, vector<string>{addr, value});
        }
        else if (command == "PRINT") {
            // Handle PRINT with quoted string
            size_t start = instrLine.find('(');
            size_t end = instrLine.rfind(')');
            if (start != string::npos && end != string::npos) {
                string content = instrLine.substr(start + 1, end - start - 1);
                instructions.emplace_back(PRINT, vector<string>{content});
            }
        }
    }
    
    return instructions;
}

class Screen {
private:
    vector<Process*> allProcesses;
    queue<Process*> globalQueue;
    bool cpusActive = false;
    bool processGeneratorActive = false;
    int processCounter = 1;
    vector<thread> cpuThreads;
    thread generatorThread;
    mutex queueMutex;
    mutex processListMutex;
    
    MemoryManager* memoryManager;

    // for vmstat
    atomic<unsigned long long> idleCpuTicks{0};
    atomic<unsigned long long> activeCpuTicks{0};
    atomic<unsigned long long> numPagedIn{0};
    atomic<unsigned long long> numPagedOut{0};

    void processGenerator() {
        int cyclesSinceLastGen = 0;
        
        if (config.minMemPerProc > config.maxOverallMem) {
            cout << "\n========================================\n";
            cout << "ERROR: Configuration Invalid!\n";
            cout << "Processes require " << config.minMemPerProc << " bytes\n";
            cout << "System only has " << config.maxOverallMem << " bytes\n";
            cout << "All processes will deadlock immediately.\n";
            cout << "Process generation disabled.\n";
            cout << "========================================\n\n";
            
            processGeneratorActive = false;
            return;
        }

        while (processGeneratorActive) {
            this_thread::sleep_for(chrono::seconds(1));
            cyclesSinceLastGen++;
            
            if (cyclesSinceLastGen >= config.batchFreq) {
                {
                    lock_guard<mutex> qLock(queueMutex);
                    lock_guard<mutex> pLock(processListMutex);
                    
                    string name = "p" + to_string(processCounter);

                    if (config.minMemPerProc > config.maxOverallMem) {
                        cout << "WARNING: Process " << name 
                            << " cannot fit in memory (needs " << config.minMemPerProc
                            << " bytes, system has " << config.maxOverallMem 
                            << " bytes). Will deadlock immediately.\n";
                    }
                    
                    Process* newProcess = new Process(processCounter, name, 
                        config.maxCommand, config.minCommand,
                        config.minMemPerProc, config.maxMemPerProc);
                    
                    allProcesses.push_back(newProcess);
                    globalQueue.push(newProcess);
                    
                    processCounter++;
                }
                cyclesSinceLastGen = 0;
            }
        }
    }

    // handle page fault for a process
    bool handlePageFault(Process* process, size_t pageNum) {
        if (process == nullptr) return false;
        
        process->pageFaultCount++;
        
        // try to allocate the page
        auto [success, frameIdx] = memoryManager->allocatePage(process->pID, pageNum);
        
        if (success) {
            // page loaded successfully
            process->addPageToMemory(pageNum, frameIdx);
            numPagedIn++;
            return true;
        }
        
        // memory is full - need page replacement
        if (memoryManager->isFull()) {
            // Select victim using FIFO
            auto [victimPID, victimPage] = memoryManager->selectVictimPage();
            
            if (victimPID == -1) {
                // no victim found (shouldn't happen)
                return false;
            }
            
            // find the victim process
            Process* victimProcess = nullptr;
            {
                lock_guard<mutex> lock(processListMutex);
                for (auto p : allProcesses) {
                    if (p->pID == victimPID) {
                        victimProcess = p;
                        break;
                    }
                }
            }
            
            // evict victim page
            if (victimProcess != nullptr) {
                memoryManager->evictPage(victimPID, victimPage, victimProcess);
                numPagedOut++;
            }
            
            // now try to allocate again
            auto [success2, frameIdx2] = memoryManager->allocatePage(process->pID, pageNum);
            if (success2) {
                process->addPageToMemory(pageNum, frameIdx2);
                numPagedIn++;
                return true;
            }
        }
        
        return false;  // failed to load page
    }
    
    // check and handle page faults before executing instruction
    bool checkAndHandlePageFaults(Process* curr, int instrIndex) {
        if (curr == nullptr || instrIndex >= curr->instructions.size()) {
            return true;  // Nothing to check
        }

        if (curr->isDeadlocked()) {
            return false;
        }
        
        Instruction& inst = curr->instructions[instrIndex];
        
        // determine which pages are needed for this instruction
        vector<size_t> pagesNeeded;
        
        switch (inst.type) {
            case DECLARE:
            case ADD:
            case SUBTRACT:
            case PRINT: {
                // need symbol table page (page 0)
                size_t symbolTablePage = 0;
                pagesNeeded.push_back(symbolTablePage);
                break;
            }
            
            case READ: {
                // need both symbol table AND the memory address page
                size_t symbolTablePage = 0;
                pagesNeeded.push_back(symbolTablePage);
                
                string addressStr = inst.params[1];
                try {
                    size_t address = stoull(addressStr, nullptr, 16);
                    size_t memoryPage = curr->getPageNumber(address);
                    pagesNeeded.push_back(memoryPage);
                } catch (...) {
                    // invalid address will be caught during execution
                }
                break;
            }
            
            case WRITE: {
                // need symbol table AND destination memory page
                size_t symbolTablePage = 0;
                pagesNeeded.push_back(symbolTablePage);
                
                string addressStr = inst.params[0];
                try {
                    size_t address = stoull(addressStr, nullptr, 16);
                    size_t memoryPage = curr->getPageNumber(address);
                    pagesNeeded.push_back(memoryPage);
                } catch (...) {
                    // invalid address will be caught during execution
                }
                break;
            }
            
            case SLEEP:
            case FOR_LOOP:
                // These don't access memory
                return true;
        }
        
        // check each needed page and handle page faults
        bool allPagesLoaded = true;
        for (size_t pageNum : pagesNeeded) {
            if (!curr->isPageInMemory(pageNum)) {
                // page fault!
                bool loaded = handlePageFault(curr, pageNum);
                if (!loaded) {
                    // failed to load page - instruction must retry
                    curr->consecutivePageFaults++;
                    allPagesLoaded = false;
                    break;
                }
            }
        }

        if (allPagesLoaded) {
            curr->consecutivePageFaults = 0; // reset on success
            return true;
        }
        
        return false;
    }

    void fcfs(int coreID) {
        while (cpusActive) {
            Process* curr = nullptr;

            {
                lock_guard<mutex> lock(queueMutex);
                if (!globalQueue.empty()) {
                    curr = globalQueue.front();
                    globalQueue.pop();
                    curr->coreAssigned = coreID;
                }
            }
            
            if (curr) {
                
                while (curr->currentInstruction < curr->totalInstruction && cpusActive && !curr->hasMemoryError && !curr->isDeadlocked()) {

                    // check if needed pages are loaded
                    bool pagesLoaded = checkAndHandlePageFaults(curr, curr->currentInstruction);

                    if (pagesLoaded) {
                        curr->executeInstruction(curr->currentInstruction);
                        curr->currentInstruction++;
                        activeCpuTicks++;

                        // check if finished or has error
                        if (curr->currentInstruction >= curr->totalInstruction || curr->hasMemoryError) {
                            curr->isFinished = true;
                            curr->endTime = time(nullptr);
                            curr->coreAssigned = -1;
                            
                            memoryManager->deallocateMemory(curr->pID);
                            curr->pagesInMemory.clear();
                            curr->pageTable.clear();
                        }

                        // delay
                        if (config.delayTime == 0)
                            this_thread::sleep_for(chrono::milliseconds(10));
                        else if (config.delayTime > 0) {
                            this_thread::sleep_for(chrono::milliseconds(max(1, config.delayTime - 800)));
                        }
                        
                    } else {
                        idleCpuTicks++;
                        this_thread::sleep_for(chrono::milliseconds(1));
                        
                        if (curr->isDeadlocked()) {
                            curr->hasMemoryError = true;
                            curr->memoryErrorAddress = "DEADLOCK: Insufficient memory";
                            curr->isFinished = true;
                            curr->endTime = time(nullptr);
                            curr->coreAssigned = -1;
                            
                            memoryManager->deallocateMemory(curr->pID);
                            curr->pagesInMemory.clear();
                            curr->pageTable.clear();
                            break;  
                        }
                    }
                }
                
                // ✅ HANDLE DEADLOCK OUTSIDE LOOP TOO
                if (curr->isDeadlocked() && !curr->isFinished) {
                    curr->hasMemoryError = true;
                    curr->memoryErrorAddress = "DEADLOCK: Insufficient memory";
                    curr->isFinished = true;
                    curr->endTime = time(nullptr);
                    curr->coreAssigned = -1;
                    
                    memoryManager->deallocateMemory(curr->pID);
                    curr->pagesInMemory.clear();
                    curr->pageTable.clear();
                }
                
            } else {
                // No process available
                idleCpuTicks++;
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    }

    void roundRobin(int coreID) {
        while (cpusActive) {
            Process* curr = nullptr;

            {
                lock_guard<mutex> lock(queueMutex);
                if (!globalQueue.empty()) {
                    curr = globalQueue.front();
                    globalQueue.pop();
                    curr->coreAssigned = coreID;
                }
            }

            if (curr) {
                int quantum = 0;

                while (quantum < config.timeQuantum 
                    && curr->currentInstruction < curr->totalInstruction 
                    && cpusActive && !curr->hasMemoryError && !curr->isDeadlocked()) {
                    
                    // check page faults
                    bool pagesLoaded = checkAndHandlePageFaults(curr, curr->currentInstruction);
                    
                    if (pagesLoaded) {
                        // execute instruction
                        curr->executeInstruction(curr->currentInstruction);
                        curr->currentInstruction++;
                        quantum++;
                        activeCpuTicks++;
                        
                        // check if finished
                        if (curr->currentInstruction >= curr->totalInstruction || curr->hasMemoryError) {
                            curr->isFinished = true;
                            curr->endTime = time(nullptr);
                            curr->coreAssigned = -1;
                            
                            memoryManager->deallocateMemory(curr->pID);
                            curr->pagesInMemory.clear();
                            curr->pageTable.clear();
                        }
                        
                        // Delay
                        if (config.delayTime == 0) {
                            this_thread::sleep_for(chrono::milliseconds(10));
                        } else if (config.delayTime > 0) {
                            this_thread::sleep_for(chrono::milliseconds(max(1, config.delayTime - 800)));
                        }
                        
                    } else {

                        idleCpuTicks++;
                        this_thread::sleep_for(chrono::milliseconds(1));
                        

                        if (curr->isDeadlocked()) {
                            curr->hasMemoryError = true;
                            curr->memoryErrorAddress = "DEADLOCK: Insufficient memory";
                            curr->isFinished = true;
                            curr->endTime = time(nullptr);
                            curr->coreAssigned = -1;
                            
                            memoryManager->deallocateMemory(curr->pID);
                            curr->pagesInMemory.clear();
                            curr->pageTable.clear();
                            break; 
                        }
                    }
                }

                if (curr->isDeadlocked() && !curr->isFinished) {
                    curr->hasMemoryError = true;
                    curr->memoryErrorAddress = "DEADLOCK: Insufficient memory";
                    curr->isFinished = true;
                    curr->endTime = time(nullptr);
                    curr->coreAssigned = -1;
                    
                    memoryManager->deallocateMemory(curr->pID);
                    curr->pagesInMemory.clear();
                    curr->pageTable.clear();
                }
                
                if (!curr->isFinished && !curr->isDeadlocked()) {
                    lock_guard<mutex> lock(queueMutex);
                    curr->coreAssigned = -1;
                    globalQueue.push(curr);
                }
                
            } else {
                idleCpuTicks++;
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    }

public:
    Screen() {
        memoryManager = nullptr;
    }
    
    void initializeMemoryManager() {
        if (memoryManager == nullptr) {
            memoryManager = new MemoryManager(config.maxOverallMem, config.memPerFrame);
        }
    }

    void startCPUs() {
        if (cpusActive) return;

        cpusActive = true;
        
        if (config.schedulingAlgorithm == "\"fcfs\"" || config.schedulingAlgorithm == "fcfs") {
            cout << "CPUs started with FCFS algorithm.\n";
            for (int i = 0; i < config.numCPU; i++) {
                cpuThreads.emplace_back(&Screen::fcfs, this, i);
            }
        } 
        else if (config.schedulingAlgorithm == "\"rr\"" || config.schedulingAlgorithm == "rr") {
            cout << "CPUs started with Round Robin (Quantum: " << config.timeQuantum << ").\n";
            for (int i = 0; i < config.numCPU; i++) {
                cpuThreads.emplace_back(&Screen::roundRobin, this, i);
            }
        }
    }

    void createProcess(const string& name, size_t memSize = 0) {
        if (!is_initialized) {
            printNotInitialized();
            return;
        }
        
        lock_guard<mutex> qLock(queueMutex);
        lock_guard<mutex> pLock(processListMutex);
        
        for (auto p : allProcesses) {
            if (p->name == name) {
                cout << "Error: Process with name '" << name << "' already exists!\n";
                return;
            }
        }
        
        // Use provided memory size or random
        if (memSize == 0) {
            memSize = (rand() % (config.maxMemPerProc - config.minMemPerProc + 1) + config.minMemPerProc);
        }

        if (memSize > config.maxOverallMem) {
            cout << "ERROR: Process requires " << memSize 
                << " bytes but system only has " << config.maxOverallMem 
                << " bytes total. Cannot create process.\n";
            return;
        }
        
        int pID = allProcesses.size() + 1;
        Process* newProcess = new Process(pID, name, config.maxCommand, config.minCommand,
            config.minMemPerProc, config.maxMemPerProc);
        newProcess->memoryRequired = memSize;
        
        allProcesses.push_back(newProcess);
        globalQueue.push(newProcess);
        
        cout << "Process " << name << " (ID: " << pID << ") created with "
             << newProcess->totalInstruction << " instructions requiring " 
             << memSize << " bytes of memory." << endl;
    }

    void createProcessWithInstructions(const string& name, size_t memSize, const string& instructionStr) {
        if (!is_initialized) {
            printNotInitialized();
            return;
        }
        
        if (!isValidMemorySize(memSize)) {
            cout << "Invalid memory allocation. Must be power of 2 in range [64, 65536].\n";
            return;
        }
        
        vector<Instruction> instructions = parseInstructions(instructionStr);
        
        if (instructions.empty() || instructions.size() > 50) {
            cout << "Invalid command. Instruction count must be between 1 and 50.\n";
            return;
        }
        
        lock_guard<mutex> qLock(queueMutex);
        lock_guard<mutex> pLock(processListMutex);
        
        for (auto p : allProcesses) {
            if (p->name == name) {
                cout << "Error: Process with name '" << name << "' already exists!\n";
                return;
            }
        }
        
        int pID = allProcesses.size() + 1;
        Process* newProcess = new Process(pID, name, memSize, instructions);
        
        allProcesses.push_back(newProcess);
        globalQueue.push(newProcess);
        
        cout << "Process " << name << " (ID: " << pID << ") created with "
             << instructions.size() << " custom instructions requiring " 
             << memSize << " bytes of memory." << endl;
    }
    
    void screenList() {
        lock_guard<mutex> lock(processListMutex);
        
        if (allProcesses.empty()) {
            cout << "No processes exist right now\n";
            return;
        }

        int runningProcesses = 0;
        for (auto p : allProcesses) {
            if (!p->isFinished && p->coreAssigned != -1) runningProcesses++;
        }
        
        int cpuUsed = runningProcesses;
        int cpuAvail = config.numCPU - cpuUsed;
        float cpuUtilization = (cpuUsed / (float)config.numCPU) * 100.0f;

        cout << "\nCPU Utilization: " << fixed << setprecision(2) << cpuUtilization << "%\n";
        cout << "Cores used: " << cpuUsed << "\n";
        cout << "Cores available: " << cpuAvail << "\n\n";
        
        cout << "---------------------------------------------\n";
        cout << "Running processes:\n";
        bool hasRunning = false;
        for (auto p : allProcesses) {
            if (!p->isFinished) {
                hasRunning = true;
                struct tm timeinfo;
                localtime_s(&timeinfo, &p->startTime);
                
                char dateBuffer[32];
                strftime(dateBuffer, sizeof(dateBuffer), "(%m/%d/%Y %I:%M:%S%p)", &timeinfo);

                string coreStr = (p->coreAssigned == -1) ? "N/A" : to_string(p->coreAssigned);
                
                cout << left << setw(15) << p->name 
                     << setw(35) << dateBuffer
                     << "Core: " << setw(10) << coreStr
                     << p->currentInstruction << "/" << p->totalInstruction << "\n";
            }
        }
        if (!hasRunning) {
            cout << "(No running processes)\n";
        }
        
        cout << "\nFinished processes:\n";
        bool hasFinished = false;
        for (auto p : allProcesses) {
            if (p->isFinished) {
                hasFinished = true;
                struct tm startInfo, endInfo;
                localtime_s(&startInfo, &p->startTime);
                localtime_s(&endInfo, &p->endTime);
                
                char startBuffer[32], endBuffer[32];
                strftime(startBuffer, sizeof(startBuffer), "(%m/%d/%Y %I:%M:%S%p)", &startInfo);
                strftime(endBuffer, sizeof(endBuffer), "(%m/%d/%Y %I:%M:%S%p)", &endInfo);
                
                cout << left << setw(15) << p->name 
                     << setw(35) << startBuffer
                     << setw(35) << endBuffer
                     << p->currentInstruction << "/" << p->totalInstruction << "\n";
            }
        }
        if (!hasFinished) {
            cout << "(No finished processes)\n";
        }
        cout << "---------------------------------------------\n";
    }

    void schedulerStart() {
        if (!is_initialized) {
            printNotInitialized();
            return;
        }

        if (processGeneratorActive) {
            cout << "Process generator is already running!\n";
            return;
        }

        processGeneratorActive = true;
        cout << "Scheduler started! Auto-generating processes every " 
             << config.batchFreq << " cycles.\n";
        
        generatorThread = thread(&Screen::processGenerator, this);
    }

    void schedulerStop() {
        if (!processGeneratorActive) {
            cout << "Process generator is not running!\n";
            return;
        }
        
        processGeneratorActive = false;
        cout << "Stopping process generation...\n";
        
        if (generatorThread.joinable()) {
            generatorThread.join();
        }
        
        cout << "Process generation stopped. CPUs continue running.\n";
    }

    // vmstat print
    void printVMStat() {
        if (!is_initialized || memoryManager == nullptr) {
            cout << "System not initialized or memory manager not available.\n";
            return;
        }
        
        lock_guard<mutex> lock(processListMutex);
        
        size_t totalMem = config.maxOverallMem;
        size_t usedMem = memoryManager->getUsedMemory();
        size_t freeMem = memoryManager->getFreeMemory();
        
        unsigned long long totalCpuTicks = idleCpuTicks.load() + activeCpuTicks.load();
        unsigned long long idleTicks = idleCpuTicks.load();
        unsigned long long activeTicks = activeCpuTicks.load();
        
        unsigned long long pagedIn = numPagedIn.load();
        unsigned long long pagedOut = numPagedOut.load();
        
        // show total page faults
        int totalPageFaults = 0;
        for (auto p : allProcesses) {
            totalPageFaults += p->pageFaultCount.load();
        }
        
        int activeProcesses = 0;
        int inactiveProcesses = 0;
        
        for (auto p : allProcesses) {
            if (p->isFinished || p->hasMemoryError) {
                inactiveProcesses++;
            } else {
                activeProcesses++;
            }
        }
        
        cout << "\n===========================================\n";
        cout << "           VMSTAT - MEMORY STATISTICS          \n";
        cout << "===========================================\n";
        
        cout << "Total Memory:     " << setw(10) << totalMem << " bytes\n";
        cout << "Used Memory:      " << setw(10) << usedMem << " bytes\n";
        cout << "Free Memory:      " << setw(10) << freeMem << " bytes\n";
        
        cout << "\n-------------------------------------------\n";
        
        cout << "Total Pages:      " << setw(10) << memoryManager->getTotalFrames() << "\n";
        cout << "Used Pages:       " << setw(10) << memoryManager->getNumPagesUsed() << "\n";
        cout << "Free Pages:       " << setw(10) << memoryManager->getNumPagesFree() << "\n";
        
        cout << "\n-------------------------------------------\n";
        
        cout << "Idle CPU Ticks:   " << setw(10) << idleTicks << "\n";
        cout << "Active CPU Ticks: " << setw(10) << activeTicks << "\n";
        cout << "Total CPU Ticks:  " << setw(10) << totalCpuTicks << "\n";
        
        cout << "\n-------------------------------------------\n";
        
        // show page fault statistics
        cout << "Total Page Faults:" << setw(10) << totalPageFaults << "\n";
        cout << "Num Paged In:     " << setw(10) << pagedIn << "\n";
        cout << "Num Paged Out:    " << setw(10) << pagedOut << "\n";
        
        cout << "\n-------------------------------------------\n";
        
        cout << "Active Processes:   " << setw(8) << activeProcesses << "\n";
        cout << "Inactive Processes: " << setw(8) << inactiveProcesses << "\n";
        cout << "===========================================\n";
    }

    void printProcessSMI() {
        if (!is_initialized || memoryManager == nullptr) {
            cout << "System not initialized or memory manager not available.\n";
            return;
        }
        
        lock_guard<mutex> lock(processListMutex);
        
        // FIX: Calculate CPU util based on active vs idle ticks (NOT instant core occupancy)
        unsigned long long totalTicks = idleCpuTicks.load() + activeCpuTicks.load();
        float cpuUtil = 0.0f;
        if (totalTicks > 0) {
            cpuUtil = (activeCpuTicks.load() / (float)totalTicks) * 100.0f;
        }
        
        // Memory information
        size_t totalMem = config.maxOverallMem;
        size_t usedMem = memoryManager->getUsedMemory();
        float memUtil = (usedMem / (float)totalMem) * 100.0f;
        
        // Convert to MiB for display
        float usedMiB = usedMem / 1048576.0f;
        float totalMiB = totalMem / 1048576.0f;
        
        // Get process memory usage
        map<int, int> processMemUsage = memoryManager->getProcessMemoryUsage();
        
        // Print header
        cout << "\n--------------------------------------\n";
        cout << "| PROCESS-SMI V01.00  Driver Version: 01.00 |\n";
        cout << "--------------------------------------\n";
        cout << "CPU-Util: " << fixed << setprecision(0) << cpuUtil << "%\n";
        cout << "Memory Usage: " << fixed << setprecision(0) << usedMiB << "MiB / " 
            << totalMiB << "MiB\n";
        cout << "Memory Util: " << fixed << setprecision(0) << memUtil << "%\n";
        cout << "\n======================================\n";
        cout << "Running processes and memory usage:\n";
        cout << "--------------------------------------\n";
        
        // List all running processes with memory
        bool hasProcesses = false;
        for (auto p : allProcesses) {
            if (!p->isFinished && !p->hasMemoryError) {
                hasProcesses = true;

                // show if deadlock
                string status = p->isDeadlocked() ? " [DEADLOCKED]" : "";

                int pages = 0;
                if (processMemUsage.find(p->pID) != processMemUsage.end()) {
                    pages = processMemUsage[p->pID];
                }
                
                float memMiB = (pages * config.memPerFrame) / 1048576.0f;
                
                cout << left << setw(20) << (p->name + status)
                    << fixed << setprecision(0) << memMiB << "MiB\n";
            }
        }
        
        if (!hasProcesses) {
            cout << "(No running processes)\n";
        }
        
        cout << "--------------------------------------\n";
    }

    void processScreen(string processName) {
        Process* targetProcess = nullptr;
        {
            lock_guard<mutex> lock(processListMutex);
            for (auto p : allProcesses) {
                if (p->name == processName) {
                    targetProcess = p;
                    break;
                }
            }
        }

        if (targetProcess == nullptr) {
            cout << "Process " << processName << " not found.\n";
            return;
        }

        if (targetProcess->hasMemoryError) {
            struct tm errorInfo;
            localtime_s(&errorInfo, &targetProcess->errorTime);
            char timeBuffer[16];
            strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &errorInfo);
            
            cout << "Process " << processName << " shut down due to memory access violation error "
                << "that occurred at " << timeBuffer << ". " 
                << targetProcess->memoryErrorAddress << " invalid.\n";
            return;
        }

        if (targetProcess->isFinished) {
            cout << "Process " << processName << " has already finished and can no longer be accessed.\n";
            return;
        }

        cout << "\n----------------------------------------------\n";
        cout << "Process: " << targetProcess->name;
        if (targetProcess->isFinished) {
            cout << " (Finished)";
        }
        cout << "\n";
        cout << "Entering process screen. Type 'process-smi' for details or 'exit' to return.\n";
        cout << "----------------------------------------------\n";

        string processCommand;
        bool inProcessScreen = true;

        while (inProcessScreen) {
            cout << "\nroot:\\" << processName << "> ";
            getline(cin, processCommand);

            if (processCommand == "process-smi") {
                cout << "\n----------------------------------------------\n";
                cout << "Process name: " << targetProcess->name << "\n";
                cout << "ID: " << targetProcess->pID << "\n";
                
                struct tm timeinfo;
                localtime_s(&timeinfo, &targetProcess->startTime);
                char dateBuffer[32];
                strftime(dateBuffer, sizeof(dateBuffer), "%m/%d/%Y %I:%M:%S%p", &timeinfo);
                cout << "Created: " << dateBuffer << "\n";
                
                // show memory and page information
                cout << "\nMemory Usage: " << targetProcess->memoryRequired << " bytes\n";
                cout << "Total Pages Needed: " << targetProcess->getTotalPages() << "\n";
                cout << "Pages in Memory: " << targetProcess->pagesInMemory.size() << "\n";
                cout << "Page Faults: " << targetProcess->pageFaultCount.load() << "\n";
                
                if (!targetProcess->outputLog.empty()) {
                    cout << "\nLogs:\n";
                    for (const auto& log : targetProcess->outputLog) {
                        cout << log << "\n";
                    }
                    cout << "\n";
                } else {
                    cout << "\nLogs:\n(No output yet)\n\n";
                }
                
                cout << "Current instruction line: " << targetProcess->currentInstruction << "\n";
                cout << "Lines of code: " << targetProcess->totalInstruction << "\n";
                
                string coreStr = (targetProcess->coreAssigned == -1) ? "N/A" : to_string(targetProcess->coreAssigned);
                cout << "Core: " << coreStr << "\n";
                
                if (targetProcess->isFinished) {
                    cout << "\nStatus: Finished!\n";
                } else if (targetProcess->coreAssigned != -1) {
                    cout << "\nStatus: Running\n";
                } else {
                    cout << "\nStatus: Waiting\n";
                }
                cout << "----------------------------------------------\n";
            }
            else if (processCommand == "exit") {
                inProcessScreen = false;
                cout << "\nReturning to main menu...\n";
                cout << "----------------------------------------------\n";
            }
            else if (!processCommand.empty()) {
                cout << "Unknown command. Available commands: process-smi, exit\n";
            }
        }
    }
    void reportUtil() {
        ofstream Logs("csopesy-log.txt");
        if (!Logs.is_open()) {
            cout << "Error writing report.\n";
            return;
        }

        lock_guard<mutex> lock(processListMutex);
        
        if (allProcesses.empty()) {
            Logs << "No processes exist right now\n";
            Logs.close();
            return;
        }
        
        int runningProcesses = 0;
        for (auto p : allProcesses) {
            if (!p->isFinished && p->coreAssigned != -1) runningProcesses++;
        }
        
        int cpuUsed = runningProcesses;
        int cpuAvail = config.numCPU - cpuUsed;
        float cpuUtilization = (cpuUsed / (float)config.numCPU) * 100.0f;
        
        Logs << "\n------------------------------------------\n";
        Logs << "CPU Utilization: " << fixed << setprecision(2) << cpuUtilization << "%\n";
        Logs << "Cores used: " << cpuUsed << "\n";
        Logs << "Cores available: " << cpuAvail << "\n";
        
        Logs << "---------------------------------------------\n";
        Logs << "Running processes:\n";
        bool hasRunning = false;
        for (auto p : allProcesses) {
            if (!p->isFinished) {
                hasRunning = true;
                struct tm timeinfo;
                localtime_s(&timeinfo, &p->startTime);
                
                char dateBuffer[32];
                strftime(dateBuffer, sizeof(dateBuffer), "(%m/%d/%Y %I:%M:%S%p)", &timeinfo);

                string coreStr = (p->coreAssigned == -1) ? "N/A" : to_string(p->coreAssigned);
                
                Logs << left << setw(15) << p->name 
                     << setw(35) << dateBuffer
                     << "Core: " << setw(10) << coreStr
                     << p->currentInstruction << "/" << p->totalInstruction << "\n";
            }
        }
        if (!hasRunning) {
            Logs << "(No running processes)\n";
        }
        
        Logs << "\nFinished processes:\n";
        bool hasFinished = false;
        for (auto p : allProcesses) {
            if (p->isFinished) {
                hasFinished = true;
                struct tm startInfo, endInfo;
                localtime_s(&startInfo, &p->startTime);
                localtime_s(&endInfo, &p->endTime);
                
                char startBuffer[32], endBuffer[32];
                strftime(startBuffer, sizeof(startBuffer), "(%m/%d/%Y %I:%M:%S%p)", &startInfo);
                strftime(endBuffer, sizeof(endBuffer), "(%m/%d/%Y %I:%M:%S%p)", &endInfo);
                
                Logs << left << setw(15) << p->name 
                     << setw(35) << startBuffer
                     << setw(35) << endBuffer
                     << p->currentInstruction << "/" << p->totalInstruction << "\n";
            }
        }
        if (!hasFinished) {
            Logs << "(No finished processes)\n";
        }
        Logs << "---------------------------------------------\n";
        
        Logs.close();
        
        filesystem::path path_object = std::filesystem::current_path();
        string path_string = path_object.string();
        cout << "Report generated at " << path_string << "\\csopesy-log.txt" << endl;
    }

    ~Screen() {
        cpusActive = false;
        processGeneratorActive = false;
        
        if (generatorThread.joinable()) {
            generatorThread.join();
        }
        
        for (auto& t : cpuThreads) {
            if (t.joinable()) {
                t.join();
            }
        }

        for (auto p : allProcesses) {
            delete p;
        }
        
        if (memoryManager != nullptr) {
            delete memoryManager;
        }
    }
};

void initializeSystem(Screen& screen) {
    ifstream file("config.txt");
    if (!file.is_open()) {
        cout << "Error: config.txt not found.\n";
        return;
    }

    file >> ws >> ws >> ws;
    file.seekg(0);
    string key;
    while (file >> key) {
        if (key == "num-cpu") file >> config.numCPU;
        else if (key == "scheduler") file >> config.schedulingAlgorithm;
        else if (key == "quantum-cycles") file >> config.timeQuantum;
        else if (key == "batch-process-freq") file >> config.batchFreq;
        else if (key == "min-ins") file >> config.minCommand;
        else if (key == "max-ins") file >> config.maxCommand;
        else if (key == "delay-per-exec") file >> config.delayTime;
        // Memory configuration
        else if (key == "max-overall-mem") file >> config.maxOverallMem;
        else if (key == "mem-per-frame") file >> config.memPerFrame;
        else if (key == "min-mem-per-proc") file >> config.minMemPerProc;
        else if (key == "max-mem-per-proc") file >> config.maxMemPerProc;
    }

    file.close();
    is_initialized = true;

    cout << "System initialized successfully!\n";
    cout << "CPUs: " << config.numCPU
         << " | Scheduler: " << config.schedulingAlgorithm
         << " | Quantum: " << config.timeQuantum << "\n";
    cout << "Memory: " << config.maxOverallMem << " bytes | Frame Size: " 
         << config.memPerFrame << " bytes\n";
    
    // initialize memory manager
    screen.initializeMemoryManager();
    screen.startCPUs();
}

// auto-calculate memory requirement based on instructions
size_t calculateRequiredMemory(const vector<Instruction>& instructions) {
    size_t maxAddress = Process::SYMBOL_TABLE_SIZE;  // Start with symbol table (64 bytes)
    
    // scan all READ/WRITE instructions to find highest memory address
    for (const auto& inst : instructions) {
        if (inst.type == READ) {
            // READ <var> <address>
            string addressStr = inst.params[1];
            try {
                size_t address = stoull(addressStr, nullptr, 16);
                if (address > maxAddress) {
                    maxAddress = address;
                }
            } catch (...) {
                // Invalid address - will be caught during execution
            }
        }
        else if (inst.type == WRITE) {
            // WRITE <address> <value>
            string addressStr = inst.params[0];
            try {
                size_t address = stoull(addressStr, nullptr, 16);
                if (address > maxAddress) {
                    maxAddress = address;
                }
            } catch (...) {
                // invalid address - will be caught during execution
            }
        }
    }
    
    // add some padding for the value being written (2 bytes for uint16)
    maxAddress += 2;
    
    // round up to nearest valid memory size (power of 2)
    size_t memSize = 64;  // Minimum
    while (memSize < maxAddress && memSize < 65536) {
        memSize *= 2;
    }
    
    return memSize;
}

int main() {
    srand(time(0));
    string command = "";
    cout << ascii_art << header << "\n--------------------------------------\n" << flush;
    Screen screen;

    while(true) {
        cout << "\n\nroot:\\> ";
        getline(cin, command);
        
        if (command == "initialize" || command == "init") {
            if (!is_initialized) {
                initializeSystem(screen);
            } else {
                cout << "System is already initialized!\n";
            }
        }
        else if (command.find("screen -s ") == 0) {
            if (is_initialized) {
                string args = command.substr(10);
                stringstream ss(args);
                string name;
                size_t memSize = 0;
                
                ss >> name >> memSize;
                
                if (name.empty()) {
                    cout << "Please provide a process name.\n";
                } else if (memSize > 0) {
                    if (!isValidMemorySize(memSize)) {
                        cout << "Invalid memory allocation. Must be power of 2 in range [64, 65536].\n";
                    } else {
                        screen.createProcess(name, memSize);
                        clearConsole();
                        screen.processScreen(name);
                    }
                } else {
                    screen.createProcess(name);
                    clearConsole();
                    screen.processScreen(name);
                }
            }
            else {
                printNotInitialized();
            }
        }
        else if (command.find("screen -c ") == 0) {
            if (is_initialized) {
                size_t firstQuote = command.find('"');
                if (firstQuote == string::npos) {
                    cout << "Invalid format. Use: screen -c <name> [<memory>] \"<instructions>\"\n";
                    continue;
                }
                
                string beforeQuote = command.substr(10, firstQuote - 10);
                stringstream ss(beforeQuote);
                string name;
                size_t memSize = 0;  // Default to 0 (auto-calculate)
                
                ss >> name;
                if (ss >> memSize) {
                    // Memory size was provided - validate it
                    if (!isValidMemorySize(memSize)) {
                        cout << "Invalid memory allocation. Must be power of 2 in range [64, 65536].\n";
                        continue;
                    }
                }
                
                size_t lastQuote = command.rfind('"');
                if (lastQuote == firstQuote) {
                    cout << "Invalid format. Instructions must be enclosed in quotes.\n";
                    continue;
                }
                
                string instructions = command.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                
                if (name.empty()) {
                    cout << "Invalid format. Use: screen -c <name> [<memory>] \"<instructions>\"\n";
                } else {
                    // Parse instructions first to calculate required memory
                    vector<Instruction> parsedInstructions = parseInstructions(instructions);
                    
                    if (parsedInstructions.empty() || parsedInstructions.size() > 50) {
                        cout << "Invalid command. Instruction count must be between 1 and 50.\n";
                        continue;
                    }
                    
                    // auto-calculate memory if not provided
                    if (memSize == 0) {
                        memSize = calculateRequiredMemory(parsedInstructions);
                        cout << "Auto-calculated memory requirement: " << memSize << " bytes\n";
                    }
                    
                    screen.createProcessWithInstructions(name, memSize, instructions);
                }
            }
            else {
                printNotInitialized();
            }
        }
        else if (command.find("screen -r ") == 0) {
            if (is_initialized) {
                string name = command.substr(10);
                if (name.empty()) {
                    cout << "Please provide a process name.\n";
                } else {
                    clearConsole();
                    screen.processScreen(name);
                }
            }
            else {
                printNotInitialized();
            }
        }
        else if (command == "screen -ls") {
            if (is_initialized) {
                screen.screenList();
            }
            else {
                printNotInitialized();
            }
        }
        else if (command == "scheduler-start") {
            screen.schedulerStart();
        }
        else if (command == "scheduler-stop") {
            screen.schedulerStop();
        }
        else if (command == "report-util") {
            screen.reportUtil();
        }
        else if (command == "vmstat") {
            screen.printVMStat();
        }
        else if (command == "process-smi") {
            screen.printProcessSMI();
        }
        else if (command == "exit") {
            cout << "Exiting CSOPESY emulator...\n";
            break;
        }
        else if (!command.empty()) {
            cout << "Unknown command: " << command << "\n";
        }
    }
    
    return 0;
}