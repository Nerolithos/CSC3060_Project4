#ifndef DEFS_H
#define DEFS_H

#include <cstdint>
#include <string>
#include <vector>

// ==========================================
// Basic Data Structures
// ==========================================

struct CacheLine {
    uint64_t tag;
    bool valid;
    bool dirty;           // Dirty bit for Write-Back policy
    uint64_t last_access; // For LRU: Timestamp of last access
    int rrpv;             // For SRRIP: Re-Reference Prediction Value (2-bit or 3-bit)
    bool is_prefetched;   // Statistic: Track if this line was brought in by prefetcher

    CacheLine() : tag(0), valid(false), dirty(false), last_access(0), rrpv(3), is_prefetched(false) {}
};

struct CacheConfig {
    uint32_t size_kb;       // Cache capacity in KB
    uint32_t associativity; // Ways per set
    uint32_t block_size;    // Block size in Bytes
    uint32_t latency;       // Hit latency in cycles
    std::string policy_name;// Replacement policy name (e.g., "LRU", "SRRIP")
    std::string prefetcher; // Prefetcher name (e.g., "None", "NextLine")
};

#endif