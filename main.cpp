#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include "cache.h"
#include "bus.h"

void printHelp()
{
    std::cout << "Usage: ./L1simulate [options]\n"
              << "Options:\n"
              << "  -t <tracefile>  : name of parallel application (e.g. app1)\n"
              << "  -s <s>          : number of set index bits\n"
              << "  -E <E>          : associativity\n"
              << "  -b <b>          : number of block bits\n"
              << "  -o <outfile>    : output file for logging\n"
              << "  -h              : print this help\n";
}

struct SimulationParams
{
    std::string baseTraceName;
    long long int setIndexBits;
    long long int associativity;
    long long int blockBits;
    std::string outFile;
};

SimulationParams parseArgs(long long int argc, char *argv[])
{
    SimulationParams params;
    params.setIndexBits = 5;  // Default: 32 sets
    params.associativity = 2; // Default: 2-way set associative
    params.blockBits = 5;     // Default: 32-byte block size

    for (long long int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
        {
            params.baseTraceName = argv[++i];
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            params.setIndexBits = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-E") == 0 && i + 1 < argc)
        {
            params.associativity = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
        {
            params.blockBits = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            params.outFile = argv[++i];
        }
        else if (strcmp(argv[i], "-h") == 0)
        {
            printHelp();
            exit(0);
        }
    }

    return params;
}

struct TraceEntry
{
    bool isWrite;
    uint32_t address;
};

std::vector<TraceEntry> readTraceFile(const std::string &filename)
{
    std::vector<TraceEntry> entries;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open trace file " << filename << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(file, line))
    {
        TraceEntry entry;
        char type;
        uint32_t address;
        if (sscanf(line.c_str(), "%c %x", &type, &address) == 2)
        {
            entry.isWrite = (type == 'W');
            entry.address = address;
            entries.push_back(entry);
        }
    }

    return entries;
}

int main(int argc, char *argv[])
{
    SimulationParams params = parseArgs(argc, argv);
    if (params.baseTraceName.empty())
    {
        std::cerr << "Error: Base trace name not specified\n";
        printHelp();
        return 1;
    }

    const long long int numCores = 4;
    uint64_t globalCycle = 0; // Single global cycle counter
    std::vector<Cache> caches;
    caches.reserve(numCores); // Pre-allocate space for all caches
    std::vector<std::vector<TraceEntry>> traces(numCores);

    bool debugmode = false;

    // Create the bus
    Bus bus(globalCycle);
    bus.setDebugMode(debugmode); // Enable debug output for the bus

    // Read traces and initialize caches
    for (long long int core = 0; core < numCores; ++core)
    {
        std::string traceFile = params.baseTraceName + "_proc" + std::to_string(core) + ".trace";
        traces[core] = readTraceFile(traceFile);
        caches.emplace_back(params.setIndexBits, params.associativity, params.blockBits, core, globalCycle);
        caches[core].setBus(&bus);       // Connect cache to the bus
        bus.registerCache(caches[core]); // Register cache with the bus
        caches[core].setDebugMode(debugmode);
    }

    // Simulate each core
    std::vector<uint64_t> totalInstructions(numCores, 0);
    std::vector<uint64_t> idleCycles(numCores, 0);
    std::vector<uint64_t> execCycles(numCores, 0);
    std::vector<size_t> currentInstructionIndex(numCores, 0);
    bool allTracesComplete = false;

    // Simulate all cores simultaneously
    while (!allTracesComplete)
    {
        allTracesComplete = true;

        // Update bus state at the start of each cycle
        if (bus.getRemainingCycles() == 1)
        {
            caches[bus.getCurrentRequestingCore()].stats.execCycles++;
            totalInstructions[bus.getCurrentRequestingCore()]++;
            currentInstructionIndex[bus.getCurrentRequestingCore()]++;
        }
        bus.updateBusState();
        // Process each core in order of cache ID (for bus transaction priority)
        for (long long int core = 0; core < numCores; ++core)
        {
            // Skip if this core has completed its trace
            if (currentInstructionIndex[core] >= traces[core].size())
            {
                continue;
            }

            allTracesComplete = false;
            const auto &entry = traces[core][currentInstructionIndex[core]];

            long long int result;
            if (entry.isWrite)
            {
                result = caches[core].write(entry.address, core);
            }
            else
            {
                result = caches[core].read(entry.address, core);
            }

            // Update cycle counts based on result
            switch (result)
            {
            case 0:                                 // Hit
                caches[core].stats.execCycles += 1; // 1 cycle for hit
                if (entry.isWrite)
                {
                    caches[core].stats.writeCount++;
                }
                else
                {
                    caches[core].stats.readCount++;
                }
                totalInstructions[core]++;
                currentInstructionIndex[core]++;
                break;
            case 1:                                 // Miss
                caches[core].stats.execCycles += 1; // 1 cycle for the operation
                if (entry.isWrite)
                {
                    caches[core].stats.writeCount++;
                }
                else
                {
                    caches[core].stats.readCount++;
                }
                break;
            case -1:                             // Bus busy with another core
                caches[core].stats.idleCycles++; // Core is idle waiting for bus
                break;
            case 2: // Bus in progress for this core
                // No need to increment idle cycles as the core is waiting for its own transaction
                caches[core].stats.execCycles++;
                break;
            }
        }

        // Increment global cycle after processing all cores
        // std::cout << "Global Cycle: " << globalCycle << std::endl;
        globalCycle++;
    }

    // Print simulation parameters
    std::ofstream outFile(params.outFile);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open output file " << params.outFile << std::endl;
        return 1;
    }

    outFile << "Simulation Parameters:\n";
    outFile << "Trace Prefix: " << params.baseTraceName << "\n";
    outFile << "Set Index Bits: " << params.setIndexBits << "\n";
    outFile << "Associativity: " << params.associativity << "\n";
    outFile << "Block Bits: " << params.blockBits << "\n";
    outFile << "Block Size (Bytes): " << (1 << params.blockBits) << "\n";
    outFile << "Number of Sets: " << (1 << params.setIndexBits) << "\n";
    outFile << "Cache Size (KB per core): " << ((1 << params.setIndexBits) * params.associativity * (1 << params.blockBits)) / 1024 << "\n";
    outFile << "MESI Protocol: Enabled\n";
    outFile << "Write Policy: Write-back, Write-allocate\n";
    outFile << "Replacement Policy: LRU\n";
    outFile << "Bus: Central snooping bus\n\n";

    // Print per-core statistics
    for (long long int core = 0; core < numCores; ++core)
    {
        const auto &stats = caches[core].getStats();
        outFile << "Core " << core << " Statistics:\n";
        outFile << "Total Instructions: " << totalInstructions[core] << "\n";
        outFile << "Total Reads: " << stats.readCount << "\n";
        outFile << "Total Writes: " << stats.writeCount << "\n";
        outFile << "Total Execution Cycles: " << stats.execCycles << "\n";
        outFile << "Total Idle Cycles: " << stats.idleCycles << "\n";
        outFile << "Cache Hits: " << stats.hitCount << "\n";
        outFile << "Cache Misses: " << stats.missCount << "\n";
        outFile << "Cache Miss Rate: " << std::fixed << std::setprecision(2) << (stats.missCount * 100.0 / (stats.readCount + stats.writeCount)) << "%\n";
        outFile << "Cache Evictions: " << stats.evictionCount << "\n";
        outFile << "Writebacks: " << stats.writebackCount << "\n";
        outFile << "Bus Invalidations: " << stats.invalidationCount << "\n";
        outFile << "Data Traffic (Bytes): " << stats.busTrafficBytes << "\n\n";
    }

    long long maximum_exec_cycles = 0;
    for (long long int core = 0; core < numCores; ++core) {
        if (caches[core].getStats().execCycles > maximum_exec_cycles) {
            maximum_exec_cycles = caches[core].getStats().execCycles;
        }
    }
    outFile << "Maximum Execution Cycles: " << maximum_exec_cycles << "\n";

    // Print bus statistics
    bus.printStats(outFile);

    outFile.close();
    return 0;
}