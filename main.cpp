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
#include <algorithm> // for std::remove
#include <queue> // were using queues

void clearConsole() {
#ifdef _WIN32
    system("cls");   // Windows
#else
    system("clear"); // Linux / Mac
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
std::string header = "Welcome to CSOPESY Emulator!\n\nGroup Developers:\nCastillo, Marvien Angel\nHerrera, Mikaela Gabrielle\nJimenez, Jaztin Jacob\nRegindin, Sean Adrien\n\nLast Updated: 11-05-2025\n";
#define MAX_PROCESS 120

using namespace std;
typedef struct {
    int numCPU;
    string schedulingAlgorithm;
    int timeQuantum;
    int batchFreq;
    int minCommand;
    int maxCommand;
    int delayTime;
} Config;

// global variables
bool is_initialized = false;
Config config;

// Instruction types enumeration
enum InstructionType {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR_LOOP
};

// Base Instruction class
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
        }
        return result;
    }
};

class Process{
public:
    string name;
    int pID;
    int coreAssigned;
    int totalInstruction;
    int currentInstruction;
    bool isFinished;
    time_t startTime;
    time_t endTime;

    // instructions of the process yup
    vector<Instruction> instructions;
    map<string, uint16_t> memory; // Variable storage
    vector<string> outputLog; // Store PRINT outputs

private:
    // Helper function to create random instructions
    void generateInstructions(int count) {
        for (int i = 0; i < count; ++i) {
            int instrType = rand() % 6; // 6 types of instructions
            switch (instrType) {
                case 0: // PRINT
                    instructions.emplace_back(PRINT, vector<string>{"\"Hello world from " + name + "!\""});
                    break;
                case 1: // DECLARE
                    instructions.emplace_back(DECLARE, vector<string>{"var" + to_string(i), to_string(rand() % 100)});
                    break;
                case 2: { // ADD
                    int prevIdx = (i > 0) ? (i - 1) : 0;
                    instructions.emplace_back(ADD, vector<string>{
                        "var" + to_string(i), 
                        "var" + to_string(prevIdx), 
                        to_string(rand() % 50)
                    });
                    break;
                }
                case 3: { // SUBTRACT
                    int prevIdx = (i > 0) ? (i - 1) : 0;
                    instructions.emplace_back(SUBTRACT, vector<string>{
                        "var" + to_string(i), 
                        "var" + to_string(prevIdx), 
                        to_string(rand() % 30)
                    });
                    break;
                }
                case 4: // SLEEP
                    instructions.emplace_back(SLEEP, vector<string>{to_string(rand() % 5 + 1)});
                    break;
                case 5: // FOR_LOOP
                    instructions.emplace_back(FOR_LOOP, vector<string>{to_string(rand() % 3 + 2)});
                    break;
            }
        }
    }
public:
    // constructor
    Process(int pID, string name,int maxIns,int minIns){
        this->pID = pID;
        this->name = name;
        this->coreAssigned = -1;
        this->totalInstruction = rand() % (maxIns - minIns + 1) + minIns;
        this->currentInstruction = 0; // initialize
        isFinished= false; 
        this->startTime = time(nullptr); // to get the curr date and time (ctime library)
    
        // generate random instructions
        generateInstructions(this->totalInstruction);
    }

    // Execute a single instruction
    void executeInstruction(int index) {
        if (index >= instructions.size()) return;
        
        Instruction& inst = instructions[index];
        
        switch(inst.type) {
            case PRINT: {
                // Store output with timestamp and core
                string msg = inst.params[0];
                // Remove quotes if present
                if (msg.front() == '"' && msg.back() == '"') {
                    msg = msg.substr(1, msg.length() - 2);
                }
                
                // Get current timestamp
                time_t now = time(nullptr);
                struct tm timeinfo;
                localtime_s(&timeinfo, &now);
                char timestamp[32];
                strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", &timeinfo);
                
                // Format: (timestamp) Core:X "message"
                string logEntry = "(" + string(timestamp) + ") Core:" 
                                + to_string(coreAssigned) + " \"" + msg + "\"";
                outputLog.push_back(logEntry);
                break;
            }
            
            case DECLARE: {
                string var = inst.params[0];
                uint16_t value = 0;
                try {
                    value = (uint16_t)stoi(inst.params[1]);
                } catch (const invalid_argument&) {
                    value = 0;
                } catch (const out_of_range&) {
                    value = 0;
                }
                memory[var] = value;
                break;
            }
            
            case ADD: {
                string var1 = inst.params[0];
                
                // Ensure var1 exists in memory (auto-declare if not)
                if (memory.find(var1) == memory.end()) {
                    memory[var1] = 0;
                }
                
                // Get var2 value (variable or literal)
                uint16_t val2 = 0;
                if (memory.find(inst.params[1]) != memory.end()) {
                    val2 = memory[inst.params[1]];
                } else {
                    try {
                        val2 = (uint16_t)stoi(inst.params[1]);
                    } catch (...) {
                        memory[inst.params[1]] = 0;
                        val2 = 0;
                    }
                }
                
                // Get var3 value (variable or literal)
                uint16_t val3 = 0;
                if (memory.find(inst.params[2]) != memory.end()) {
                    val3 = memory[inst.params[2]];
                } else {
                    try {
                        val3 = (uint16_t)stoi(inst.params[2]);
                    } catch (...) {
                        memory[inst.params[2]] = 0;
                        val3 = 0;
                    }
                }
                
                // Perform addition with clamping
                uint32_t result = (uint32_t)val2 + (uint32_t)val3;
                if (result > UINT16_MAX) result = UINT16_MAX;
                
                memory[var1] = (uint16_t)result;
                break;
            }
            
            case SUBTRACT: {
                string var1 = inst.params[0];
                
                // Ensure var1 exists in memory (auto-declare if not)
                if (memory.find(var1) == memory.end()) {
                    memory[var1] = 0;
                }
                
                // Get var2 value
                uint16_t val2 = 0;
                if (memory.find(inst.params[1]) != memory.end()) {
                    val2 = memory[inst.params[1]];
                } else {
                    try {
                        val2 = (uint16_t)stoi(inst.params[1]);
                    } catch (...) {
                        memory[inst.params[1]] = 0;
                        val2 = 0;
                    }
                }
                
                // Get var3 value
                uint16_t val3 = 0;
                if (memory.find(inst.params[2]) != memory.end()) {
                    val3 = memory[inst.params[2]];
                } else {
                    try {
                        val3 = (uint16_t)stoi(inst.params[2]);
                    } catch (...) {
                        memory[inst.params[2]] = 0;
                        val3 = 0;
                    }
                }
                
                // Perform subtraction with clamping (no negative)
                int32_t result = (int32_t)val2 - (int32_t)val3;
                if (result < 0) result = 0;
                
                memory[var1] = (uint16_t)result;
                break;
            }
            
            case SLEEP: {
                // Sleep is handled by the scheduler (CPU relinquishes)
                break;
            }
            
            case FOR_LOOP: {
                try {
                    int repeats = stoi(inst.params[0]);
    
                } catch (...) {
                    // Invalid repeat count, skip
                }
                break;
            }
        }
    }

    
};

void printNotInitialized() {
    cout << "Please initialize the system first!" << endl;
}

class Screen {
private:
    vector<Process*> allProcesses;        // All processes (for tracking/viewing)
    queue<Process*> globalQueue;          // GLOBAL READY QUEUE
    bool cpusActive = false;
    bool processGeneratorActive = false;
    int processCounter = 1;
    vector<thread> cpuThreads;
    thread generatorThread;
    mutex queueMutex;
    mutex processListMutex;

    // Auto-generate processes
    void processGenerator() {
        int cyclesSinceLastGen = 0;
        
        while (processGeneratorActive) {
            
            this_thread::sleep_for(chrono::seconds(1));
            cyclesSinceLastGen++;
            
            // Generate process when cycles reach batch-process-freq
            if (cyclesSinceLastGen >= config.batchFreq) {
                {
                    lock_guard<mutex> qLock(queueMutex);
                    lock_guard<mutex> pLock(processListMutex);
                    
                    string name = "p";
                    if (processCounter < 10) {
                        name += "0" + to_string(processCounter);
                    } else {
                        name += to_string(processCounter);
                    }
                    
                    Process* newProcess = new Process(processCounter, name, config.maxCommand, config.minCommand);
                    allProcesses.push_back(newProcess);
                    globalQueue.push(newProcess);
                    
                    processCounter++;
                }
                cyclesSinceLastGen = 0;  // Reset counter
            }
        }
    }

    // FCFS: Each core picks from queue and runs to completion
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
                while (curr->currentInstruction < curr->totalInstruction && cpusActive) {
                    // Use delay-per-exec for instruction execution
                if (config.delayTime == 0)
                    this_thread::sleep_for(chrono::seconds(config.delayTime) + chrono::milliseconds(10));
                else if (config.delayTime > 0) {
                    this_thread::sleep_for(chrono::seconds(config.delayTime) - chrono::milliseconds(800));
                }
                    
                    curr->executeInstruction(curr->currentInstruction);
                    curr->currentInstruction++;

                    if (curr->currentInstruction >= curr->totalInstruction) {
                        curr->isFinished = true;
                        curr->endTime = time(nullptr);
                        curr->coreAssigned = -1;
                    }
                }
            } else {
                if (config.delayTime == 0) {
                    this_thread::sleep_for(chrono::seconds(config.delayTime) + chrono::milliseconds(10));
                } else if (config.delayTime > 0) {
                    this_thread::sleep_for(chrono::seconds(config.delayTime) - chrono::milliseconds(800));
                }
            }
        }
    }

    // Round Robin: Each core picks from queue and runs for quantum cycles
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
                       && cpusActive) {
                    
                    // Use delay-per-exec for instruction execution
                    if (config.delayTime == 0) {
                        this_thread::sleep_for(chrono::seconds(config.delayTime) + chrono::milliseconds(10));
                    } else if (config.delayTime > 0) {
                        this_thread::sleep_for(chrono::seconds(config.delayTime) - chrono::milliseconds(800));
                    }

                    curr->executeInstruction(curr->currentInstruction);
                    curr->currentInstruction++;
                    quantum++;

                    if (curr->currentInstruction >= curr->totalInstruction) {
                        curr->isFinished = true;
                        curr->endTime = time(nullptr);
                        curr->coreAssigned = -1;
                    }
                }

                if (!curr->isFinished) {
                    lock_guard<mutex> lock(queueMutex);
                    curr->coreAssigned = -1;
                    globalQueue.push(curr);
                }
            } else {
                if (config.delayTime == 0) {
                    this_thread::sleep_for(chrono::seconds(config.delayTime) + chrono::milliseconds(10));
                } else if (config.delayTime > 0) {
                    this_thread::sleep_for(chrono::seconds(config.delayTime) - chrono::milliseconds(800));
                }
            }
        }
    }

public: 

    // Start CPUs on initialization
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

    // Manual process creation (screen -s <name>)
    void createProcess(const string& name) {
        if (!is_initialized) {
            printNotInitialized();
            return;
        }
        
        lock_guard<mutex> qLock(queueMutex);
        lock_guard<mutex> pLock(processListMutex);
        
        // Check if process with this name already exists
        for (auto p : allProcesses) {
            if (p->name == name) {
                cout << "Error: Process with name '" << name << "' already exists!\n";
                return;
            }
        }
        
        int pID = allProcesses.size() + 1;
        Process* newProcess = new Process(pID, name, config.maxCommand, config.minCommand);
        
        allProcesses.push_back(newProcess);
        globalQueue.push(newProcess);  // Add to queue
        
        cout << "Process " << name << " (ID: " << pID << ") created with "
             << newProcess->totalInstruction << " instructions and added to queue." << endl;
    }
    
    // Show process/es
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

        //checker for when process is already finished
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
                cout << "\nProcess name: " << targetProcess->name << "\n";
                cout << "ID: " << targetProcess->pID << "\n";
                
                if (!targetProcess->outputLog.empty()) {
                    cout << "Logs:\n";
                    for (const auto& log : targetProcess->outputLog) {
                        cout << log << "\n";
                    }
                    cout << "\n";
                } else {
                    cout << "Logs:\n\n";
                }
                
                cout << "Current instruction line: " << targetProcess->currentInstruction << "\n";
                cout << "Lines of code: " << targetProcess->totalInstruction << "\n";
                
                if (targetProcess->isFinished) {
                    cout << "\nFinished!\n";
                }
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
    }

    file.close();
    is_initialized = true;

    cout << "System initialized successfully!\n";
    cout << "CPUs: " << config.numCPU
         << " | Scheduler: " << config.schedulingAlgorithm
         << " | Quantum: " << config.timeQuantum << "\n";
    
    screen.startCPUs();
}

int main(){
    srand(time(0));
    string command = "";
    cout << ascii_art << header << "\n--------------------------------------\n" << flush;
    Screen screen;

    while(true){
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
                string name = command.substr(10);
                if (name.empty()) {
                    cout << "Please provide a process name.\n";
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
