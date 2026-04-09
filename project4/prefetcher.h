#ifndef PREFETCHER_H
#define PREFETCHER_H

#include "interfaces.h"

Prefetcher* createPrefetcher(std::string name, uint32_t block_size);

class NoPrefetcher : public Prefetcher {
public:
    std::vector<uint64_t> calculatePrefetch(uint64_t current_addr, bool miss) override {
        (void)current_addr;
        (void)miss;
        return {};
    }
    std::string getName() override { return "None"; }
};

class NextLinePrefetcher : public Prefetcher {
private:
    uint32_t block_size;
public:
    explicit NextLinePrefetcher(uint32_t bs) : block_size(bs) {}
    std::vector<uint64_t> calculatePrefetch(uint64_t current_addr, bool miss) override;
    std::string getName() override { return "NextLine"; }
};

class StridePrefetcher : public Prefetcher {
private:
    uint32_t block_size;
    bool has_last_block = false;
    uint64_t last_block = 0;
    int64_t last_stride = 0;
    uint32_t confidence = 0;

public:
    explicit StridePrefetcher(uint32_t bs) : block_size(bs) {}
    std::vector<uint64_t> calculatePrefetch(uint64_t current_addr, bool miss) override;
    std::string getName() override { return "Stride"; }
};

#endif
