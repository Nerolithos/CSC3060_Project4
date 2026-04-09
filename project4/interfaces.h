#ifndef INTERFACES_H
#define INTERFACES_H

#include "defs.h"
#include <vector>
#include <string>

// ==========================================
// Interface: Memory Object
// Represents any level in hierarchy (L1, L2, Main Memory)
// ==========================================
class MemoryObject {
public:
    virtual ~MemoryObject() {}
    
    /**
     * Core access function.
     * @param addr Physical address
     * @param type 'r' for read, 'w' for write
     * @param cycle Current simulation cycle
     * @return Total cycles consumed by this access (including lower levels on miss)
     */
    virtual int access(uint64_t addr, char type, uint64_t cycle) = 0;
    
    virtual std::string getName() = 0;
    virtual void printStats() = 0;
};

// ==========================================
// Interface: Replacement Policy
// ==========================================
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() {}
    
    // Called when a line is hit. Update metadata (e.g., timestamp, RRPV).
    virtual void onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) = 0;
    
    // Called when a new line is inserted (Miss fill). Initialize metadata.
    virtual void onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) = 0;
    
    // Called when set is full. Return the index (way) of the victim to evict.
    virtual int getVictim(std::vector<CacheLine>& set) = 0;
};

// ==========================================
// Interface: Prefetcher
// ==========================================
class Prefetcher {
public:
    virtual ~Prefetcher() {}
    
    /**
     * Calculate addresses to prefetch based on current access.
     * @param current_addr Current accessed address
     * @param miss True if the current access was a cache miss
     * @return List of addresses to prefetch
     */
    virtual std::vector<uint64_t> calculatePrefetch(uint64_t current_addr, bool miss) = 0;
    
    virtual std::string getName() = 0;
};

#endif