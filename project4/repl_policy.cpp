#include "repl_policy.h"

// =========================================================
// LRU
// Uses last_access timestamp: highest = MRU, lowest = LRU.
// =========================================================

void LRUPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    if (way >= 0 && way < (int)set.size()) {
        set[way].last_access = cycle;
    }
}

void LRUPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    if (way >= 0 && way < (int)set.size()) {
        set[way].last_access = cycle;
    }
}

int LRUPolicy::getVictim(std::vector<CacheLine>& set) {
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

// =========================================================
// SRRIP (Static Re-Reference Interval Prediction)
// 2-bit RRPV per line: 0 = near-immediate, 3 = distant.
// Insert at RRPV=2; promote to 0 on hit; evict RRPV==3.
// If no RRPV==3 candidate, age all lines by +1 and retry.
// =========================================================

void SRRIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)cycle;
    if (way >= 0 && way < (int)set.size()) {
        set[way].rrpv = 0; // promote to near-immediate re-reference
    }
}

void SRRIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)cycle;
    if (way >= 0 && way < (int)set.size()) {
        set[way].rrpv = 2; // insert with long re-reference interval (max-1)
    }
}

int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    // Keep aging until we find a line with RRPV == 3 (distant re-reference).
    while (true) {
        for (int i = 0; i < (int)set.size(); ++i) {
            if (set[i].rrpv == 3) return i;
        }
        // No victim yet — age every line by 1.
        for (int i = 0; i < (int)set.size(); ++i) {
            if (set[i].rrpv < 3) set[i].rrpv++;
        }
    }
}

// =========================================================
// BIP (Bimodal Insertion Policy)
// Same victim selection as LRU (min last_access).
// On miss: insert at LRU position most of the time (last_access=0),
// but every `throttle`-th insertion promote to MRU to preserve
// some recency for hot lines in scan-heavy workloads.
// On hit: always promote to MRU.
// =========================================================

void BIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    if (way >= 0 && way < (int)set.size()) {
        set[way].last_access = cycle; // promote to MRU
    }
}

void BIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    if (way >= 0 && way < (int)set.size()) {
        insertion_counter++;
        if (insertion_counter % throttle == 0) {
            set[way].last_access = cycle; // occasional MRU insertion
        } else {
            set[way].last_access = 0; // biased toward LRU insertion
        }
    }
}

int BIPPolicy::getVictim(std::vector<CacheLine>& set) {
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

ReplacementPolicy* createReplacementPolicy(std::string name) {
    if (name == "SRRIP") return new SRRIPPolicy();
    if (name == "BIP")   return new BIPPolicy();
    return new LRUPolicy();
}
