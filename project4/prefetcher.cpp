#include "prefetcher.h"

// =========================================================
// NextLine Prefetcher
// On every access (hit or miss) prefetch the immediately
// following cache block.  No stride detection needed — it
// exploits the dominant stride-1 (sequential) pattern.
// =========================================================

std::vector<uint64_t> NextLinePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)miss; // prefetch on every access for maximum sequential coverage
    uint64_t block_base = (current_addr / block_size) * block_size;
    return {block_base + block_size};
}

// =========================================================
// Stride Prefetcher
// Tracks the stride between consecutive block addresses.
// Prefetches one block ahead once the same stride has been
// observed at least `CONF_THRESHOLD` times in a row.
// =========================================================

std::vector<uint64_t> StridePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)miss;
    const uint32_t CONF_THRESHOLD = 2;

    uint64_t current_block = current_addr / block_size;

    if (!has_last_block) {
        last_block = current_block;
        has_last_block = true;
        return {};
    }

    int64_t stride = (int64_t)current_block - (int64_t)last_block;
    last_block = current_block;

    if (stride == 0) return {}; // same block re-access, ignore

    if (stride == last_stride) {
        if (confidence < CONF_THRESHOLD + 1) confidence++;
    } else {
        last_stride = stride;
        confidence = 1;
    }

    if (confidence >= CONF_THRESHOLD) {
        uint64_t prefetch_addr = (current_block + stride) * block_size;
        return {prefetch_addr};
    }

    return {};
}

Prefetcher* createPrefetcher(std::string name, uint32_t block_size) {
    if (name == "NextLine") return new NextLinePrefetcher(block_size);
    if (name == "Stride")   return new StridePrefetcher(block_size);
    return new NoPrefetcher();
}
