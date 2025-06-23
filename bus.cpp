#include "bus.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

// Initialize static member
std::ofstream Bus::debugFile;

Bus::Bus(uint64_t &cycle) : globalCycle(cycle), debugMode(false), isBusy(false), remainingCycles(0), currentRequestingCore(-1)
{
    resetStats();
    // Open debug file once at initialization
    debugFile.open("debug.txt", std::ios::out | std::ios::app); // Use append mode instead of truncate
}

Bus::~Bus()
{
    if (debugFile.is_open())
    {
        debugFile.close();
    }
}

void Bus::debugPrint(const std::string &msg) const
{
    if (!debugMode)
        return; // Early return if debug mode is disabled
    debugFile << "[Bus Cycle " << globalCycle << "] " << msg << std::endl;
}

void Bus::resetStats()
{
    stats = BusStats{};
}

void Bus::registerCache(Cache &cache)
{
    caches.push_back(std::ref(cache));
    debugPrint("New cache registered with the bus");
}

void Bus::updateBusState()
{
    if (isBusy)
    {
        remainingCycles--;
        if (remainingCycles <= 0)
        {
            isBusy = false;
            currentRequestingCore = -1;
            debugPrint("Bus transaction completed");
        }
    }
}

void Bus::addRemainingCycles(long long int cycles, long long int coreId)
{
    remainingCycles += cycles;
    if(isBusy) return;
    isBusy = true;
    currentRequestingCore = coreId;
}

bool Bus::broadcastTransaction(BusTransactionType type, uint32_t address, long long int requestingCore)
{
    // If bus is busy, transaction cannot be processed
    if (isBusy && currentRequestingCore != requestingCore)
    {
        std::stringstream ss;
        ss << "Transaction rejected because bus is busy for core " << requestingCore;
        debugPrint(ss.str());
        return false;
    }

    BusTransaction transaction{type, address, requestingCore, globalCycle};

    // Set bus as busy and mark the requesting core
    isBusy = true;
    currentRequestingCore = requestingCore;

    // Note: We no longer set remaining cycles here
    // The cache will set the appropriate cycle count during processTransaction

    debugPrint("Starting new bus transaction");
    return processTransaction(transaction);
}

bool Bus::processTransaction(const BusTransaction &transaction)
{
    // Update statistics based on transaction type
    stats.totalTransactions++;
    switch (transaction.type)
    {
    case BusTransactionType::BusRd:
        stats.busRdTransactions++;
        break;
    case BusTransactionType::BusRdX:
        stats.busRdXTransactions++;
        break;
    case BusTransactionType::BusUpgr:
        stats.busUpgrTransactions++;
        break;
    }

    bool dataFromOtherCache = false;
    bool data_requested = true;

    // Notify all other caches about the transaction
    for (size_t i = 0; i < caches.size(); i++)
    {
        if (static_cast<long long int>(i) != transaction.requestingCore)
        {
            bool isWrite = (transaction.type == BusTransactionType::BusRdX ||
                            transaction.type == BusTransactionType::BusUpgr);
            if (isWrite)
            {
                data_requested = false;
            }
            dataFromOtherCache |= (caches[i].get().processBusTransaction(transaction.address, isWrite, transaction.requestingCore, data_requested));
            if (dataFromOtherCache)
                data_requested = false;
        }
    }

    // Convert transaction type to string for debug output
    std::string typeStr;
    switch (transaction.type)
    {
    case BusTransactionType::BusRd:
        typeStr = "BusRd";
        break;
    case BusTransactionType::BusRdX:
        typeStr = "BusRdX";
        break;
    case BusTransactionType::BusUpgr:
        typeStr = "BusUpgr";
        break;
    default:
        typeStr = "Unknown";
        break;
    }

    std::stringstream ss;
    ss << "Processed " << typeStr
       << " transaction for address 0x" << std::hex << transaction.address << std::dec
       << " from core " << transaction.requestingCore;
    debugPrint(ss.str());

    return dataFromOtherCache;
}

const BusStats &Bus::getStats() const
{
    return stats;
}

void Bus::printStats(std::ostream& out) const
{
    out << "\nBus Statistics:\n";
    out << "Total Transactions: " << stats.totalTransactions << "\n";
    out << "BusRd Transactions: " << stats.busRdTransactions << "\n";
    out << "BusRdX Transactions: " << stats.busRdXTransactions << "\n";
    out << "BusUpgr Transactions: " << stats.busUpgrTransactions << "\n";
    out << "Total Bus Traffic (Bytes): " << stats.totalBusTraffic << "\n";
}
