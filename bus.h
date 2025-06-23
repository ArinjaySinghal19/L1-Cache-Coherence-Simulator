#ifndef BUS_H
#define BUS_H

#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include "cache.h"

// Bus transaction types for MESI protocol
enum class BusTransactionType
{
    BusRd,   // Bus Read - Request for shared copy
    BusRdX,  // Bus Read Exclusive - Request for exclusive copy
    BusUpgr, // Bus Upgrade - Request to upgrade to exclusive
};

// Bus transaction structure
struct BusTransaction
{
    BusTransactionType type;
    uint32_t address;
    long long int requestingCore;
    uint64_t timestamp;
};

// Bus statistics
struct BusStats
{
    uint64_t totalTransactions;
    uint64_t busRdTransactions;
    uint64_t busRdXTransactions;
    uint64_t busUpgrTransactions;
    uint64_t totalBusTraffic;
};

class Bus
{
private:
    std::vector<std::reference_wrapper<Cache>> caches; // Store references to Cache objects
    uint64_t &globalCycle; // Reference to global cycle counter (non-const)
    bool debugMode;
    bool isBusy;                         // Flag to indicate if bus is currently busy
    long long int remainingCycles;       // Number of cycles remaining for current transaction
    long long int currentRequestingCore; // Core that currently has the bus
    static std::ofstream debugFile;      // Static file stream for debug output

    void debugPrint(const std::string &msg) const;

public:
    Bus(uint64_t &cycle);
    ~Bus(); // Add destructor to close debug file
    void setDebugMode(bool enable) { debugMode = enable; }
    BusStats stats;
    // Core bus operations
    void registerCache(Cache &cache); // Take a reference
    bool broadcastTransaction(BusTransactionType type, uint32_t address, long long int requestingCore);
    bool processTransaction(const BusTransaction &transaction);
    void updateBusState();                         // New method to update bus state each cycle
    void addRemainingCycles(long long int cycles, long long int coreId); // New method to set remaining cycles

    // Statistics
    const BusStats &getStats() const;
    void resetStats();
    void printStats(std::ostream& out = std::cout) const;

    // Helper methods
    uint64_t getCurrentCycle() const { return globalCycle; }
    bool isBusyNow() const { return isBusy; }
    long long int getRemainingCycles() const { return remainingCycles; }
    long long int getCurrentRequestingCore() const { return currentRequestingCore; }
};

#endif // BUS_H