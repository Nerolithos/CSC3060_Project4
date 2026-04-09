#ifndef REPL_POLICY_H
#define REPL_POLICY_H

#include "interfaces.h"

ReplacementPolicy* createReplacementPolicy(std::string name);

class LRUPolicy : public ReplacementPolicy {
public:
    void onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) override;
    void onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) override;
    int getVictim(std::vector<CacheLine>& set) override;
};

class SRRIPPolicy : public ReplacementPolicy {
public:
    void onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) override;
    void onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) override;
    int getVictim(std::vector<CacheLine>& set) override;
};

class BIPPolicy : public ReplacementPolicy {
private:
    uint64_t insertion_counter = 0;
    uint32_t throttle = 32;

public:
    void onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) override;
    void onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) override;
    int getVictim(std::vector<CacheLine>& set) override;
};

#endif
