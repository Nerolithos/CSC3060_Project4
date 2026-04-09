#include "repl_policy.h"

// =========================================================
// TODO: Task 1 / Task 3 replacement policies
// Implement LRU first, then extend with SRRIP / BIP.
// =========================================================

void LRUPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    // Mark the hit line as most recently used by updating its timestamp.
    if (way >= 0 && way < (int)set.size()) {
        set[way].last_access = cycle;
    }
}

void LRUPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    // Newly inserted line becomes most recently used.
    if (way >= 0 && way < (int)set.size()) {
        set[way].last_access = cycle;
    }
}

int LRUPolicy::getVictim(std::vector<CacheLine>& set) {
    // Choose the line with the smallest last_access (least recently used).
    int victim = 0;
    uint64_t min_ts = set[0].last_access;
    for (int i = 1; i < (int)set.size(); ++i) {
        if (set[i].last_access < min_ts) {
            min_ts = set[i].last_access;
            victim = i;
        }
    }
    return victim;
}

void SRRIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)set;
    (void)way;
    (void)cycle;
    // TODO: typically promote the line to RRPV=0.
}

void SRRIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)set;
    (void)way;
    (void)cycle;
    // TODO: insert with a long re-reference interval, e.g. RRPV=2.
}

int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    (void)set;
    // TODO: search for RRPV==3, otherwise age all lines and retry.
    return 0;
}

void BIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)set;
    (void)way;
    (void)cycle;
    // TODO: hits still become MRU.
}

void BIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)set;
    (void)way;
    (void)cycle;
    // TODO: mostly insert at LRU position, but occasionally insert at MRU.
    // Hint: use insertion_counter and throttle.
}

int BIPPolicy::getVictim(std::vector<CacheLine>& set) {
    (void)set;
    // TODO: BIP usually uses the same victim selection as LRU.
    return 0;
}

ReplacementPolicy* createReplacementPolicy(std::string name) {
    if (name == "SRRIP") return new SRRIPPolicy();
    if (name == "BIP") return new BIPPolicy();
    return new LRUPolicy();
}
