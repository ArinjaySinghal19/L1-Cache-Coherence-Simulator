#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>

// Forward declaration of Bus class
class Bus;

// Cache line states for MESI protocol
enum class CacheState
{
    MODIFIED,
    EXCLUSIVE,
    SHARED,
    INVALID
};

// Convert CacheState to string for debugging
inline std::string stateToString(CacheState state)
{
    switch (state)
    {
    case CacheState::MODIFIED:
        return "M";
    case CacheState::EXCLUSIVE:
        return "E";
    case CacheState::SHARED:
        return "S";
    case CacheState::INVALID:
        return "I";
    default:
        return "?";
    }
}

// Cache line structure
struct CacheLine
{
    uint32_t tag;
    CacheState state;
    bool dirty;                 // Added dirty bit
    uint64_t lastAccessTime;    // For LRU replacement
    std::vector<uint32_t> data; // Actual data stored in the cache line
};

// Cache set structure
struct CacheSet
{
    std::vector<CacheLine> lines;
    long long int accessCount;
};

// Cache statistics
struct CacheStats
{
    long long int readCount;
    long long int writeCount;
    long long int hitCount;
    long long int missCount;
    long long int evictionCount;
    long long int writebackCount;
    long long int invalidationCount;
    long long int busTrafficBytes;
    long long int execCycles;
    long long int idleCycles;
    long long int totalCycles;
};

class Cache
{
private:
    uint32_t numSets;
    uint32_t associativity;
    uint32_t blockSize;
    uint32_t setIndexBits;
    uint32_t blockBits;
    uint32_t tagBits;
    std::vector<CacheSet> sets;

    uint64_t &globalCycle;          // Reference to global cycle counter (non-const)
    bool debugMode;                 // Added to control debug output
    Bus *bus;                       // Pointer to the bus
    long long int cacheId;          // Added to identify which cache instance this is
    static std::ofstream debugFile; // Static file stream for debug output

    // Helper functions
    uint32_t getSetIndex(uint32_t address);
    uint32_t getTag(uint32_t address);
    uint32_t getBlockOffset(uint32_t address);
    long long int findLRULine(long long int setIndex);
    void updateLRU(long long int setIndex, long long int lineIndex);
    void writeBackToMemory(long long int setIndex, long long int lineIndex);
    void debugPrint(const std::string &msg) const; // Added debug print helper
public:
    CacheStats stats;
    Cache(uint32_t setIndexBits, uint32_t associativity, uint32_t blockBits, long long int id, uint64_t &cycle);
    ~Cache(); // Add destructor to close debug file
    void setDebugMode(bool enable) { debugMode = enable; }
    void setBus(Bus *busPtr) { bus = busPtr; }
    long long int read(uint32_t address, long long int coreId);
    long long int write(uint32_t address, long long int coreId);
    bool processBusTransaction(uint32_t address, bool isWrite, long long int requestingCore, bool data_requested);
    const CacheStats &getStats() const;
    void resetStats();
    void printStats() const;

    // Getter methods for statistics
    uint64_t getReads() const { return stats.readCount; }
    uint64_t getWrites() const { return stats.writeCount; }
    uint64_t getHits() const { return stats.hitCount; }
    uint64_t getMisses() const { return stats.missCount; }
    uint64_t getEvictions() const { return stats.evictionCount; }
    uint64_t getWritebacks() const { return stats.writebackCount; }
    uint64_t getInvalidations() const { return stats.invalidationCount; }
    uint64_t getBusTraffic() const { return stats.busTrafficBytes; }
    uint64_t getTotalCycles() const { return stats.totalCycles; }
};

#endif // CACHE_H