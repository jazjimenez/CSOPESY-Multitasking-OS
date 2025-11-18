# CSOPESY Marquee Console

## Project Information
- **Course:** CSOPESY (Introduction to Operation Systems)
- **Project:** Process Scheduling Emulator with CLI Interface

## Group Members
- Castillo, Marvien Angel
- Herrera, Mikaela Gabrielle
- Jimenez, Jaztin Jacob
- Regindin, Sean Adrien

## Entry Point
- **Main File:** main.cpp
- **Location:** CSOPESY-Emulator\main.cpp

## System Requirements
- Platform: Windows, Linux, or macOS (cross-platform compatible)
- Compiler: C++17 compatible compiler (GCC, Clang, or MSVC)
- Libraries: Standard C++ libraries with threading support

## Compilation Instructions

### Using g++ (Recommended):
```bash
g++ -std=c++17 main.cpp -o main.exe
```
## Configuration File
Change the information config.txt in the same directory if needed:
```
num-cpu 4
scheduler "rr"
quantum-cycles 5
batch-process-freq 3
min-ins 5
max-ins 15
delay-per-exec 100
```

## Running the Program
After successful compilation, run:
```bash
main.exe
```

## Features
- **Multi-core CPU Simulation:** Configurable number of processor cores
- **Scheduling Algorithms:** FCFS (First-Come-First-Served) and Round Robin with time quantum
- **Dynamic Process Generation:** Automatic process creation with customizable frequency
- **Process Isolation:** Each process maintains private memory space and variables
- **Real-time Monitoring:** Live process tracking and execution logging
- **Interactive CLI:** Comprehensive command-line interface for system control

## Available Commands
- `initialize` or `init` - Load configuration and start CPU cores
- `screen -s <name>` - Create new process and enter process screen
- `screen -r <name>` - Resume existing process screen
- `screen -ls` - List all processes (running and completed)
- `scheduler-start` - Begin automatic process generation
- `scheduler-stop` - Halt automatic process generation
- `report-util` - Generate CPU utilization report
- `exit` - Terminate the emulator

## File Structure
```
CSOPESY-Marquee-console/
├── main.cpp          # Final implementation (MAIN ENTRY POINT)
├── config.txt        # System configuration parameters
└── README.md         # Project documentation
```

## Notes
- Ensure config.txt is present in the working directory before initialization
- The emulator automatically starts CPU threads upon initialization
- Process screens provide isolated environments for individual process inspection
- Utilization reports are saved to csopesy-log.txt with timestamps and core assignments
- For optimal viewing, use a terminal with sufficient width to display process lists clearly
