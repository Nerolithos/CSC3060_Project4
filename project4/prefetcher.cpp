#include "prefetcher.h"

std::vector<uint64_t> NextLinePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)current_addr;
    (void)miss;
    std::vector<uint64_t> prefetches;

    // TODO: Task 3
    // 1. Align current_addr down to the current cache block.
    // 2. Prefetch the next sequential block.

    return prefetches;
}

std::vector<uint64_t> StridePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)current_addr;
    (void)miss;

    // TODO: Task 3
    // Suggested design:
    // 1. Track the previous accessed block.
    // 2. Compute the current stride in block units.
    // 3. If the same stride repeats enough times, prefetch the next block at that stride.
    // 4. Update last_block / last_stride / confidence.

    return {};
}

Prefetcher* createPrefetcher(std::string name, uint32_t block_size) {
    if (name == "NextLine") return new NextLinePrefetcher(block_size);
    if (name == "Stride") return new StridePrefetcher(block_size);
    return new NoPrefetcher();
}
