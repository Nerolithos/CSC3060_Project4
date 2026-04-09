#ifndef MEMORY_HIERARCHY_H
#define MEMORY_HIERARCHY_H

#include "interfaces.h"
#include "defs.h"
#include <vector>

// ==========================================
// Main Memory Simulator
// ==========================================
class MainMemory : public MemoryObject {
private:
    int latency;
    uint64_t access_count = 0;
public:
    MainMemory(int lat);
    int access(uint64_t addr, char type, uint64_t cycle) override;
    std::string getName() override { return "Main Memory"; }
    void printStats() override;
};

// ==========================================
// Cache Level Simulator (L1, L2, etc.)
// ==========================================
class CacheLevel : public MemoryObject {
private:
    std::string level_name;
    CacheConfig config;
    MemoryObject* next_level; // Pointer to the next level (L2 or Memory)
    
    // Components
    ReplacementPolicy* policy;
    Prefetcher* prefetcher;
    std::vector<std::vector<CacheLine>> sets;

    // Geometry parameters
    uint32_t offset_bits;
    uint32_t index_bits;
    uint64_t num_sets;

    // Stats
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t write_backs = 0;
    uint64_t prefetch_issued = 0;

    // Helpers
    uint64_t get_index(uint64_t addr);
    uint64_t get_tag(uint64_t addr);
    uint64_t reconstruct_addr(uint64_t tag, uint64_t index);
    void write_back_victim(const CacheLine& line, uint64_t index, uint64_t cycle);
    
    // Internal function to install a prefetched line
    void install_prefetch(uint64_t addr, uint64_t cycle);

public:
    CacheLevel(std::string name, CacheConfig cfg, MemoryObject* next);
    ~CacheLevel();
    
    int access(uint64_t addr, char type, uint64_t cycle) override;
    std::string getName() override { return level_name; }
    void printStats() override;
};

#endif