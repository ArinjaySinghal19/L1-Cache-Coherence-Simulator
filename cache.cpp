#include "cache.h"
#include "bus.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <fstream>

// Initialize static member
std::ofstream Cache::debugFile;

void Cache::debugPrint(const std::string &msg) const
{
    if (!debugMode)
        return; // Early return if debug mode is disabled

    if (!debugFile.is_open())
    {
        // Clear the file when first opening it
        debugFile.open("debug.txt", std::ios::out | std::ios::trunc);
    }
    debugFile << "Core " << cacheId << " [Cycle " << globalCycle << "]: " << msg << std::endl;
}

Cache::Cache(uint32_t setIndexBits, uint32_t associativity, uint32_t blockBits, long long int id, uint64_t &cycle)
    : numSets(0), associativity(associativity), blockSize(0), setIndexBits(setIndexBits),
      blockBits(blockBits), tagBits(0), sets(), globalCycle(cycle), debugMode(false),
      bus(nullptr), cacheId(id), stats()
{
    // Calculate number of sets and block size
    numSets = 1 << setIndexBits;
    blockSize = 1 << blockBits;
    tagBits = 32 - setIndexBits - blockBits;

    // Initialize cache sets and lines
    sets.resize(numSets);
    for (uint32_t i = 0; i < numSets; ++i)
    {
        sets[i].lines.resize(associativity);
        sets[i].accessCount = 0;
        for (uint32_t j = 0; j < associativity; ++j)
        {
            sets[i].lines[j].tag = 0;
            sets[i].lines[j].state = CacheState::INVALID;
            sets[i].lines[j].lastAccessTime = 0;
            sets[i].lines[j].data.clear();
        }
    }

    // Initialize statistics
    resetStats();
}

Cache::~Cache()
{
    if (debugFile.is_open())
    {
        debugFile.close();
    }
}

void Cache::resetStats()
{
    stats.readCount = 0;
    stats.writeCount = 0;
    stats.hitCount = 0;
    stats.missCount = 0;
    stats.evictionCount = 0;
    stats.writebackCount = 0;
    stats.invalidationCount = 0;
    stats.busTrafficBytes = 0;
    stats.idleCycles = 0;
    stats.totalCycles = 0;
}

uint32_t Cache::getSetIndex(uint32_t address)
{
    // Extract the set index bits from the address
    return (address >> blockBits) & ((1 << setIndexBits) - 1);
}

uint32_t Cache::getTag(uint32_t address)
{
    // Extract the tag bits from the address
    return address >> (setIndexBits + blockBits);
}

uint32_t Cache::getBlockOffset(uint32_t address)
{
    // Extract the block offset bits from the address
    return address & ((1 << blockBits) - 1);
}

long long int Cache::findLRULine(long long int setIndex)
{
    // Find the line with the smallest lastAccessTime (i.e., least recently used)
    uint64_t minTime = UINT64_MAX;
    long long int lruLine = 0;
    for (uint32_t i = 0; i < associativity; i++)
    {
        // If an invalid line is found, return it immediately (empty slot)
        if (sets[setIndex].lines[i].state == CacheState::INVALID)
        {
            return i;
        }
        if (sets[setIndex].lines[i].lastAccessTime < minTime)
        {
            minTime = sets[setIndex].lines[i].lastAccessTime;
            lruLine = i;
        }
    }
    return lruLine;
}

void Cache::updateLRU(long long int setIndex, long long int lineIndex)
{
    // Update the lastAccessTime for the accessed line to the current cycle
    sets[setIndex].lines[lineIndex].lastAccessTime = globalCycle;
}

void Cache::writeBackToMemory(long long int setIndex, long long int lineIndex)
{
    stats.writebackCount++;
    stats.busTrafficBytes += blockSize;
    bus->stats.totalBusTraffic += blockSize;
    // Use the bus to write back to memory
    if (bus)
    {
        bus->addRemainingCycles(100, cacheId);
    }

    sets[setIndex].lines[lineIndex].dirty = false;
}

bool Cache::processBusTransaction(uint32_t address, bool isWrite, long long int requestingCore, bool data_requested)
{
    if (requestingCore == cacheId)
        return false; // Don't process our own transactions
    uint32_t setIndex = getSetIndex(address);
    uint32_t tag = getTag(address);

    // Search for the line in the set
    for (uint32_t i = 0; i < associativity; ++i)
    {
        CacheLine &line = sets[setIndex].lines[i];

        if (line.tag == tag && line.state != CacheState::INVALID)
        {
            if (isWrite)
            {
                // Invalidate the line if it's a write transaction (BusRdX or BusUpgr)
                if (line.state == CacheState::MODIFIED)
                {
                    writeBackToMemory(setIndex, i);
                }
                line.state = CacheState::INVALID;
                debugPrint("  Line invalidated due to bus write transaction");
            }
            else
            {
                // For read transactions (BusRd), transition to SHARED state if in EXCLUSIVE or MODIFIED
                if (line.state == CacheState::EXCLUSIVE || line.state == CacheState::MODIFIED)
                {
                    if (line.state == CacheState::MODIFIED)
                    {
                        // If modified, need to write back first
                        writeBackToMemory(setIndex, i);
                    }
                    line.state = CacheState::SHARED;
                    debugPrint("  Line transitioned to SHARED due to bus read transaction");
                }
                if (data_requested)
                {
                    stats.busTrafficBytes += blockSize;
                    bus->addRemainingCycles(2 * (blockSize / 4), cacheId);
                }
            }
            return true; // We have the data
        }
    }
    return false; // We don't have the data
}

long long int Cache::read(uint32_t address, long long int coreId)
{

    if (bus->isBusyNow() && bus->getCurrentRequestingCore() == coreId)
    {
        std::stringstream ss;
        ss.str("");
        ss << "Bus is busy for core " << coreId;
        debugPrint(ss.str());
        return 2;
    }

    uint32_t setIndex = getSetIndex(address);
    uint32_t tag = getTag(address);

    std::stringstream ss;
    ss << "READ 0x" << std::hex << address << std::dec
       << " (Set: " << setIndex << ", Tag: 0x" << std::hex << tag << std::dec << ")";
    debugPrint(ss.str());

    // Search for the line in the set
    for (uint32_t i = 0; i < associativity; ++i)
    {
        CacheLine &line = sets[setIndex].lines[i];
        if (line.tag == tag && line.state != CacheState::INVALID)
        {
            // Cache hit
            stats.hitCount++;
            updateLRU(setIndex, i);

            ss.str("");
            ss << "  HIT in way " << i << " (State: " << stateToString(line.state) << ")";
            debugPrint(ss.str());
            return 0;
        }
    }

    if (bus->isBusyNow() && bus->getCurrentRequestingCore() != coreId)
    {
        std::stringstream ss;
        ss << "Bus is busy for core " << bus->getCurrentRequestingCore();
        debugPrint(ss.str());
        return -1;
    }

    // Cache miss
    stats.missCount++;
    debugPrint("  READ MISS");

    // Find a line to replace (LRU or INVALID)
    long long int replaceIdx = findLRULine(setIndex);
    CacheLine &victim = sets[setIndex].lines[replaceIdx];

    ss.str("");
    ss << "  Replacing line in way " << replaceIdx;
    if (victim.state != CacheState::INVALID)
    {
        ss << " (Old state: " << stateToString(victim.state) << ")";
    }
    debugPrint(ss.str());

    // Write back if needed (if MODIFIED)
    if (victim.state == CacheState::MODIFIED)
    {
        writeBackToMemory(setIndex, replaceIdx);
        debugPrint("  Writeback required - writing modified data to memory");
    }
    if (victim.state != CacheState::INVALID)
    {
        stats.evictionCount++;
    }
    //  Try to broadcast BusRd request on the bus
    bool dataFromOtherCache = false;
    if (bus)
    {
        dataFromOtherCache = bus->broadcastTransaction(BusTransactionType::BusRd, address, coreId);
    }

    // If no other cache has the data or bus is busy, read from main memory
    if (!dataFromOtherCache)
    {
        // Set remaining cycles for memory access (100 cycles)
        bus->addRemainingCycles(100, cacheId);
        victim.state = CacheState::EXCLUSIVE; // We're the only one with this data
        debugPrint("  Reading data from main memory - transitioning to EXCLUSIVE state");
    }
    else
    {
        victim.state = CacheState::SHARED; // Another cache had the data
        debugPrint("  Received data from another cache - transitioning to SHARED state");
    }

    stats.busTrafficBytes += blockSize;
    bus->stats.totalBusTraffic += blockSize;
    // Fill the line
    victim.tag = tag;
    victim.dirty = false;
    updateLRU(setIndex, replaceIdx);

    debugPrint("  Line filled (State: " + stateToString(victim.state) + ")");
    return 1;
}

long long int Cache::write(uint32_t address, long long int coreId)
{ // 0:hit, 1:miss, -1:bus busy, 2:bus in progress

    if (bus->isBusyNow() && bus->getCurrentRequestingCore() == coreId)
    {
        std::stringstream ss;
        ss << "Bus is busy for core " << coreId;
        debugPrint(ss.str());
        return 2;
    }

    uint32_t setIndex = getSetIndex(address);
    uint32_t tag = getTag(address);

    std::stringstream ss;
    ss << "WRITE 0x" << std::hex << address << std::dec
       << " (Set: " << setIndex << ", Tag: 0x" << std::hex << tag << std::dec << ")";
    debugPrint(ss.str());

    // Search for the line in the set
    for (uint32_t i = 0; i < associativity; ++i)
    {
        CacheLine &line = sets[setIndex].lines[i];
        if (line.tag == tag && line.state != CacheState::INVALID)
        {

            ss.str("");
            ss << "  HIT in way " << i << " (State: " << stateToString(line.state);

            // If not MODIFIED, need to upgrade (MESI)
            if (line.state != CacheState::MODIFIED)
            {
                if (bus->isBusyNow())
                {
                    debugPrint("  Bus is busy, skipping BusUpgr");
                    return -1;
                }
                debugPrint("  Sending BusUpgr message on bus");
                if (line.state == CacheState::SHARED)
                {
                    stats.invalidationCount++;
                }
                bus->broadcastTransaction(BusTransactionType::BusUpgr, address, coreId);
                ss << " -> " << stateToString(CacheState::MODIFIED) << ")";
                line.state = CacheState::MODIFIED;
            }
            else
            {
                ss << ")";
            }

            // Cache hit
            stats.hitCount++;
            updateLRU(setIndex, i);

            line.dirty = true;
            debugPrint(ss.str());
            return 0;
        }
    }

    if (bus->isBusyNow() && bus->getCurrentRequestingCore() != coreId)
    {
        std::stringstream ss;
        ss << "Bus is busy for core " << bus->getCurrentRequestingCore();
        debugPrint(ss.str());
        return -1;
    }

    // Cache miss
    stats.missCount++;
    debugPrint("  WRITE MISS");

    // Find a line to replace (LRU or INVALID)
    long long int replaceIdx = findLRULine(setIndex);
    CacheLine &victim = sets[setIndex].lines[replaceIdx];

    ss.str("");
    ss << "  Replacing line in way " << replaceIdx;
    if (victim.state != CacheState::INVALID)
    {
        ss << " (Old state: " << stateToString(victim.state) << ")";
    }
    debugPrint(ss.str());

    // Write back if needed (if MODIFIED)
    if (victim.state == CacheState::MODIFIED)
    {
        writeBackToMemory(setIndex, replaceIdx);
        debugPrint("  Writeback required - writing modified data to memory");
    }
    if (victim.state != CacheState::INVALID)
    {
        stats.evictionCount++;
    }
    // why eviction only for modified?

    bool dataFromOtherCache = false;
    if (bus)
    {
        dataFromOtherCache = bus->broadcastTransaction(BusTransactionType::BusRdX, address, coreId);
    }

    if (dataFromOtherCache)
    {
        stats.invalidationCount++;
    }
    bus->addRemainingCycles(100, cacheId);
    stats.busTrafficBytes += blockSize;
    bus->stats.totalBusTraffic += blockSize;

    // Fill the line
    victim.tag = tag;
    victim.state = CacheState::MODIFIED;
    victim.dirty = true;
    updateLRU(setIndex, replaceIdx);

    debugPrint("  Line filled (State: " + stateToString(victim.state) + ")");
    return 1;
}

const CacheStats &Cache::getStats() const
{
    // Return the cache statistics struct
    return stats;
}

void Cache::printStats() const
{
    // Print statistics in a format similar to the sample output
    std::cout << "Total Reads: " << stats.readCount << std::endl;
    std::cout << "Total Writes: " << stats.writeCount << std::endl;
    std::cout << "Total Execution Cycles: " << stats.totalCycles << std::endl;
    std::cout << "Idle Cycles: " << stats.idleCycles << std::endl;
    std::cout << "Cache Misses: " << stats.missCount << std::endl;

    // Calculate miss rate
    uint64_t totalAccesses = stats.readCount + stats.writeCount;
    double missRate = (totalAccesses > 0) ? (100.0 * static_cast<double>(stats.missCount) / totalAccesses) : 0.0;
    std::cout << "Cache Miss Rate: " << std::fixed << std::setprecision(2) << missRate << "%" << std::endl;

    std::cout << "Cache Evictions: " << stats.evictionCount << std::endl;
    std::cout << "Writebacks: " << stats.writebackCount << std::endl;
    std::cout << "Bus Invalidations: " << stats.invalidationCount << std::endl;
    std::cout << "Data Traffic (Bytes): " << stats.busTrafficBytes << std::endl;
}