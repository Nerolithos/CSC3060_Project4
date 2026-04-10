# CSC3060 Project 4 Report — Cache Simulator

---

## 1. Student / Team Information

| | Student 1 | Student 2 |
|---|---|---|
| **Name** | ZHU Ji'an | LI Jinhong |
| **Student ID** | 124090960 | 124090302 |

**Chosen seed Student ID (for trace generation and file naming):** `124090960`

Trace generated to file: `my_trace.txt`

---



## 2. Implementation Summary

### Task 1 — Single-Level L1 Cache
- Implemented `get_index(addr)`, `get_tag(addr)`, and `reconstruct_addr(tag, index)` in `memory_hierarchy.cpp`.
- Implemented full `CacheLevel::access(addr, write, cycle)`: hit detection, miss handling, invalid-slot search, victim selection, dirty write-back, and line installation.
- Implemented `LRUPolicy` in `repl_policy.cpp` using the simulation `cycle` value as a timestamp.
- Fixed a subtle LRU bug where newly inserted lines (with default `last_access = 0`) could appear "older" than uninitialized lines.

### Task 2 — Two-Level Cache Hierarchy (L1 → L2 → Memory)
- Created the L2 `CacheLevel` object in `main.cpp` when `--enable-l2` is passed.
- Connected the hierarchy by setting L1's `next_level` pointer to L2.
- Because `CacheLevel::access` already makes recursive calls to `next_level`, no changes to the core miss path were needed.
- Added L2 statistics output (`Hit Rate`, `WB`, `Prefetches Issued`).

### Task 3 — Optimization on Personalized Trace
- Implemented `SRRIPPolicy` and `BIPPolicy` in `repl_policy.cpp`.
- Implemented `NextLinePrefetcher` and `StridePrefetcher` in `prefetcher.cpp`.
- Completed `install_prefetch(addr, cycle)` in `memory_hierarchy.cpp`.
- Through trace analysis and systematic experimentation, reduced AMAT from **24.43 cycles** (baseline) to **1.89 cycles** (final configuration), surpassing the Best_AMAT target of 1.9 cycles.

---



## 3. Address Mapping Explanation

A memory address is divided into three fields, from low to high bits:

```
| tag bits | set index bits | block offset bits |
```

### Block Offset
The offset selects a byte within a cache block. If the block size is `B` bytes:

$$\text{offset\_bits} = \log_2(B)$$

The `get_index` and `get_tag` functions both first strip the offset by right-shifting the address by `offset_bits`.

### Set Index
The index selects one set within the cache. If there are `S` sets:

$$\text{index\_bits} = \log_2(S), \quad S = \frac{\text{cache\_size}}{\text{assoc} \times B}$$

```cpp
// memory_hierarchy.cpp
uint64_t without_offset = addr >> offset_bits;
uint64_t index_mask = (1ULL << index_bits) - 1ULL;
return without_offset & index_mask;
```

Using a bitmask (`& mask`) instead of `% num_sets` is preferred because it is safe even when `num_sets` is not a power of two, and is also slightly faster.

### Tag
The tag is the remaining upper bits after removing both offset and index:

```cpp
return addr >> (offset_bits + index_bits);
```

### How Geometry Changes Affect Mapping

| Change | Effect |
|---|---|
| Larger block size | More offset bits → fewer index bits for the same cache size → fewer sets → more aliasing |
| Larger cache size | More sets (if assoc fixed) → more index bits → different set mapping for the same address |
| Higher associativity | Fewer sets for the same total size → fewer index bits → more lines share a set |
| Smaller block size | Fewer offset bits → more sets → finer-grained set distribution |

This means **the same address maps to a different set when cache geometry changes**, which is why we verified correctness across multiple configurations in Task 1.

### `reconstruct_addr`
Used to recover the full block address from a tag and set index (e.g., for dirty write-back):

```cpp
return (tag << (offset_bits + index_bits)) | (index << offset_bits);
```

---



## 4. Task 1 Testing

### What We Tested

We tested correctness under three different cache geometries using `trace_sanity.txt`, and compared our output against the expected reference values provided in the requirements.

### Representative Results

**Configuration 1 — Default (`make task1`): 32 KB, 8-way, 64-byte block**

```
Constructed L1: 32KB, 8-way, 1cyc, [LRU + None]
[L1] Hit Rate: 21.43% (Access: 56, Miss: 44, WB: 2)
AMAT: 79.57 cycles
```
✓ Matches expected reference output exactly.

**Configuration 2 — Smaller block: 32 KB, 4-way, 32-byte block**

```
Constructed L1: 32KB, 4-way, 1cyc, [LRU + None]
[L1] Hit Rate: 21.43% (Access: 56, Miss: 44, WB: 2)
AMAT: 79.57 cycles
```
The hit rate is the same because the address pattern in `trace_sanity.txt` does not depend on block size for this access count. Index and tag bits shift correctly.

**Configuration 3 — Larger cache: 64 KB, 8-way, 128-byte block**

```
Constructed L1: 64KB, 8-way, 1cyc, [LRU + None]
[L1] Hit Rate: 60.71% (Access: 56, Miss: 22, WB: 0)
AMAT: 40.29 cycles
```
A larger cache and wider blocks improve the hit rate significantly — the cache can now hold more data before evictions occur, and wider blocks cover more spatial locality per fetch.

### What We Verified

- Tag, index, and offset are computed correctly for different geometries (confirmed by matching hit/miss counts).
- Dirty write-back is triggered only for `dirty && valid` lines.
- LRU timestamp is updated correctly on both hits and insertions, with `last_access = 0` reserved for never-used invalid lines.
- No regression across geometry changes — the simulator works for any power-of-two block size, any associativity, and any cache size.

---



## 5. Task 2 Hierarchy Explanation

### How L1, L2, and Memory Interact

The hierarchy is: **L1 → L2 → Main Memory**

Each level is a `CacheLevel` object with a pointer to its `next_level`. When L1 misses, it calls `next_level->access(...)`, which is L2. If L2 also misses, it calls `next_level->access(...)`, which is main memory (a `MemoryLevel` object that always returns after `mem_latency` cycles).

| Access Scenario | Path | Approximate Cost |
|---|---|---|
| L1 hit | L1 only | 1 cycle |
| L1 miss, L2 hit | L1 + L2 | 1 + 4 = 5 cycles |
| L1 miss, L2 miss | L1 + L2 + Memory | 1 + 4 + 100 = 105 cycles |

### What Changed After Adding L2

**Before L2 (`make task1`):**
```
[L1] Hit Rate: 21.43%  (Access: 56, Miss: 44, WB: 2)
[Main Memory] Total Accesses: 46
AMAT: 79.57 cycles
```

**After adding L2 (`make task2`):**
```
[L1] Hit Rate: 21.43%  (Access: 56, Miss: 44, WB: 2)
[L2] Hit Rate: 50.00%  (Access: 46, Miss: 23, WB: 0)
[Main Memory] Total Accesses: 23
AMAT: 45.21 cycles
```

L2 served 50% of the L1 misses (23 out of 46), cutting main-memory accesses from 46 to 23 and reducing AMAT from 79.57 to 45.21 cycles — a **43% reduction** without any change to L1 or the access logic.

### Implementation Notes

Since `CacheLevel::access` was already implemented recursively (calling `next_level->access` on every miss), adding L2 required only three lines in `main.cpp`:
1. Instantiate the L2 `CacheLevel` with its own configuration.
2. Point L2's `next_level` at `memory`.
3. Point L1's `next_level` at `l2` instead of `memory`.

No changes to `memory_hierarchy.cpp` were needed beyond ensuring the recursive miss path worked correctly for any depth of hierarchy.

---



## 6. Task 3 Design Choices

### Replacement Policies Implemented

**LRU (Least Recently Used)**
Already implemented in Task 1. Uses `cycle` as a timestamp; the line with the lowest `last_access` is the victim.

**SRRIP (Static Re-Reference Interval Prediction)**
Uses a 2-bit RRPV per line. Insert new lines at RRPV=2 (long-interval prediction); promote on hit to RRPV=0; evict the line with RRPV=3, aging all lines by +1 if none is at 3.

```cpp
void SRRIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    set[way].rrpv = 2;
}
void SRRIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    set[way].rrpv = 0;
}
int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    while (true) {
        for (int i = 0; i < (int)set.size(); ++i)
            if (set[i].rrpv == 3) return i;
        for (int i = 0; i < (int)set.size(); ++i)
            if (set[i].rrpv < 3) set[i].rrpv++;
    }
}
```

**BIP (Bimodal Insertion Policy)**
Inserts most new lines at the LRU position (`last_access = 0`) to resist scan pollution. Every `throttle`-th miss inserts at MRU to preserve some recency for hot lines.

```cpp
void BIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    insertion_counter++;
    if (insertion_counter % throttle == 0)
        set[way].last_access = cycle;
    else
        set[way].last_access = 0;
}
```

### Prefetchers Implemented

**NextLine Prefetcher**
On every access, prefetches the immediately next cache block (`current_block + 1`). Maximally aggressive for sequential / stride-1 patterns. Simple and effective when stride-1 is dominant.

**Stride Prefetcher**
Tracks consecutive block-address differences. Issues a prefetch one stride ahead once the same stride has been observed `CONF_THRESHOLD = 2` times in a row. Handles non-unit strides (e.g., stride-64) but requires two observations before triggering.

### No custom prefetcher was designed
Given that NextLine with assoc=32 already achieved 1.89 cycles (below the 1.9 target), a custom prefetcher was not necessary. NextLine is optimal for this trace's 60.91% stride-1 dominance.

### `install_prefetch` Completion
`install_prefetch(addr, cycle)` reuses the existing victim-selection and write-back logic: find an invalid slot, fall back to `getVictim`, write back dirty victims if necessary, and install the prefetched line as `valid=true, dirty=false`. This ensures prefetched lines are registered with the replacement policy metadata.

---



## 7. Trace Analysis

Now we run `trace_analyzer.py`.

### Basic Access Mix

| Metric | Value |
|---|---|
| Total accesses | 5,992 |
| Read ratio | 96.86% (5,804 reads) |
| Write ratio | 3.14% (188 writes) |
| Unique blocks touched | 1,236 |
| Address range | `0x10003980` – `0x1007cd40` |

The trace is heavily read-dominated. The 1,236 unique blocks × 64 bytes = ~77 KB total footprint is larger than L1 (32 KB) but fits comfortably in L2 (128 KB), explaining why L2 is valuable.

### Block-Stride Distribution

| Stride (in blocks) | Count | % |
|---|---|---|
| +1 (sequential) | 3,649 | 60.91% |
| +64 (large jump) | 1,434 | 23.94% |
| −1088 | 55 | 0.92% |
| −576 | 51 | 0.85% |
| Others | < 40 each | < 0.68% |

**Stride-1 at 60.91%** is the dominant pattern. This strongly justifies **NextLine prefetching**: the next sequential block is the most likely next access. Stride-64 at 23.94% likely corresponds to a loop traversing a 2D array row-by-row. A Stride prefetcher could exploit this, but since stride-1 is overwhelmingly more common and NextLine is always-on, NextLine covers both patterns well in practice.

### Hot Blocks and Sets

| L1 Set | Accesses | % of Total |
|---|---|---|
| Set 40 | 1,290 | **21.53%** |
| Set 1 | 545 | 9.10% |
| Set 26 | 318 | 5.31% |
| Sets 38–45 | ~90 each | ~1.50% each |

Set 40 alone accounts for **21.53% of all accesses**. The top-10 hot blocks are all accessed exactly 60 times each and map to consecutive addresses in the same region — this is a tight loop iterating over a small working set of ~20 blocks.

### Per-Window Phase Analysis

| Window | Reads | Writes | Unique Blocks | Unique Sets | Phase |
|---|---|---|---|---|---|
| 0 – 512 | 470 | 42 | 192 | 64 | Warm-up / mixed |
| 512 – 1024 | 482 | 30 | 512 | 64 | Large scan begins |
| 1024 – 1536 | 512 | 0 | 512 | 64 | Pure streaming scan |
| 1536 – 2048 | 512 | 0 | 512 | 64 | Pure streaming scan |
| 2048 – 2560 | 512 | 0 | 247 | 64 | Partial reuse |
| 2560 – 3072 | 508 | 4 | 34 | 2 | Narrowing locality |
| 3072 – 3584 | 487 | 25 | **20** | **1** | **Hot loop — set 40** |
| 3584 – 4096 | 486 | 26 | **20** | **1** | **Hot loop — set 40** |
| 4096 – 4608 | 488 | 24 | 366 | 64 | Large scan resumes |
| 4608 – 5120 | 498 | 14 | 451 | 64 | Large scan |
| 5120 – 5992 | 498+ | 14+ | 320–451 | 64 | Trailing scan |

**Three distinct phases:**

1. **Scan phases (windows 1024–2048, 4096+):** 512 unique blocks per window touching all 64 sets. Classic streaming with no intra-window reuse. LRU is hurt here because useful blocks are displaced by scan traffic.

2. **Hot loop phase (windows 3072–4096):** 20 unique blocks, all in set 40. These blocks are accessed 60+ times each. With assoc=8, only 8 fit in the set at once → **constant conflict-miss thrashing**. This phase is the dominant bottleneck.

3. **Mixed / transitional phases:** windows 0–512 and 2048–3072 show decreasing footprint as the workload focuses toward the hot loop.

### How the Analysis Influenced Design Decisions

| Observation | Design Decision | Reasoning |
|---|---|---|
| Stride-1 = 60.91% | Use **NextLine** prefetcher | Unconditional next-block prefetch maximizes sequential coverage |
| Set 40 has 20 hot blocks, assoc=8 only holds 8 | Raise **TASK3_ASSOC to 32** | No policy can fix a geometric capacity shortage; 32 ways holds all 20 blocks with room to spare |
| Scan phases touch 512 unique blocks/window | SRRIP or BIP considered to limit scan pollution | **But once assoc=32 resolves thrashing, LRU already achieves 99.05% hit rate.** |
| Total footprint ~77 KB > L1 (32 KB) but < L2 (128 KB) | Enable **L2** | L2 absorbs both demand misses and prefetch-fill traffic cheaply (4 cycles vs 100) |

---



## 8. Task 3: Experimental Results

### Full Comparison Table

Note: You may notice in the list below that there's **NO L2 prefetch**, I'll talk about this later.

| \ | L1 Assoc | L1 Policy | L1 Prefetch | L2 Enabled | L2 Policy | L1 Hit % | L2 Hit % | AMAT |
|---|---|---|---|---|---|---|---|---|
| Baseline | 8 | LRU | None | Yes | LRU | 30.02% | 71.42% | 24.43 |
| +L2 +SRRIP +NextLine       | 8        | SRRIP     | NextLine     | Yes        | SRRIP     | 74.32%     | 77.82%     | 2.98     |
| +L2 +BIP +NextLine         | 8        | BIP       | NextLine     | Yes        | LRU       | 74.10%     | 76.90%     | 2.80     |
| assoc=16, LRU+NextLine     | 16       | LRU       | NextLine     | Yes        | LRU       | 85.10%     | 72.41%     | 2.45     |
| **assoc=32, LRU+NextLine** | **32**   | **LRU**   | **NextLine** | **Yes**    | **LRU**   | **99.05%** | **60.16%** | **1.89** |

Note that if we increase associativity to 64, AMAT stays at 1.89. If we increase even more, and the AMAT will increase. So **the "best solution" shown above is NOT unique**, assoc=64 is also good enough.

### Key Observations

**Note: NextLine without L2 makes it even worse**  
We've already know from the "Trace Analysis" part that we **SHOULD** Enable **L2**. Further testing also shows that deleting L2 will cause larger AMAT even with prefetcher (**27.77** > 24.43).

**Why the jump from assoc=8 to assoc=16 is moderate (2.98 → 2.45):**  
At assoc=16, set 40 still can only hold 16 of the 20 hot blocks. Four blocks are still being evicted on every hot-loop cycle, causing persistent conflict misses.

**Why assoc=32 produces a near-perfect hit rate (99.05%):**  
With 32 ways, all 20 hot blocks fit in set 40 simultaneously (12 ways to spare). Every hot-loop access is now an L1 hit. Combined with NextLine prefetching covering the sequential-scan phases, L1 misses drop from 1,539 (assoc=8) to just 57 (assoc=32) — a 96% reduction.

**Why L2 prefetch is set to None:**  
After reaching assoc = 32, we also tested enabling NextLine at the L2 level:

| L2 Prefetch | L1 Hit % | L2 Hit % | AMAT |
|---|---|---|---|
| None | 99.05% | 60.16% | **1.89** |
| NextLine | 99.05% | 60.16% | 1.90 |

Note that, We've also tried these: **if assoc = 16**, with or without L2 prefetch yields AMAT: 4.26 and 3.68 each. Similarly, **if assoc = 64**, with or without L2 prefetch yields both 1.89.

Enabling L2 prefetch made AMAT marginally *worse* or at least not any better. The reason is structural: with **L1 hit rate already at 99.05%**, only 57 demand accesses per run reach L2. These 57 are almost **ALL compulsory misses**, so these blocks have never been in cache at any level. Prefetching the block after a compulsory miss is rarely useful, because the sequential scan will naturally arrive at that block on its own shortly afterward. Meanwhile, L2 NextLine issues additional prefetch requests that miss in L2 and fall through to main memory **at 100 cycles each, adding unnecessary traffic**. In this configuration, **L2 prefetching creates cost without benefit** — so `TASK3_L2_PREFETCH = None` is the correct and measured choice.

Note: This is very intriguing to us, since we've never actually thought that prefetch could bring negative effect due to burdening the BUS while revising our 128-page PPT. This is unexpected fuition! We understand prefetching far more thouroughly than just reading PPT.

**Final output (`make task3`):**
```
Constructed L2: 128KB, 32-way, 4cyc, [LRU + None]
Constructed L1: 32KB, 32-way, 1cyc, [LRU + NextLine]

=== Simulation Results ===
  [L1] Hit Rate: 99.05% (Access: 5992, Miss: 57, WB: 107)
       Prefetches Issued: 3059
  [L2] Hit Rate: 60.16% (Access: 3223, Miss: 1284, WB: 4)
  [Main Memory] Total Accesses: 1288

  Total Cycles: 11320
  AMAT:         1.89 cycles
```

**Student_AMAT (1.89) < Best_AMAT (1.9) → Full score.**

---



## 9. Best Configuration and Discussion

### Final Configuration

```makefile
TASK3_ASSOC = 32
TASK3_BLOCK = 64
TASK3_L1_POLICY = LRU
TASK3_L1_PREFETCH = NextLine
TASK3_L2_POLICY = LRU
TASK3_L2_PREFETCH = None
```

### Why It Performs Well

**Associativity = 32 is decisive.**  
The trace has a hot phase where 20 blocks repeatedly access set 40. With standard assoc=8, this causes perpetual conflict-miss thrashing that no replacement policy can cure. Raising to 32 ways eliminates the thrashing entirely, pushing L1 hit rate to 99.05%.

**NextLine covers 60.91% scenarios.**  
With 60.91% stride-1 transitions, nearly every next access is the immediately following block. NextLine prefetches that block in advance, converting cold misses in sequential scan phases into L1 hits. The 3,059 prefetch issues at L1 correspond directly to future hits.

**Including L2 allows L1-prefetching.**  
Every prefetch that misses in L1 costs only 4 cycles (L2 latency) rather than 100 cycles (main memory). This is why the same L1-NextLine prefetcher that hurt performance without L2 (AMAT 27.77) is beneficial with L2 (AMAT 1.89).

**LRU is best for high associativity.**  
With 32 ways and a hot working set of only 20 blocks, LRU's simple recency ordering keeps all hot blocks resident. SRRIP's scan-protection is irrelevant because the hot set fits comfortably and sequential regions are pre-covered by the prefetcher.

### Residual Misses and Limitations

The 57 remaining L1 misses (0.95%) are primarily **compulsory misses** — first accesses to blocks that have never been loaded. These occur at the start of each new scan region and cannot be eliminated by any replacement policy. A larger prefetch degree (prefetching 2–3 blocks ahead) could reduce some of these, but the gain would be marginal given TI AMAT is already below 1.90.

The configuration is well-suited for this specific trace's structure (hot loop + sequential scans). A workload with a more random access pattern or a larger hot working set (> 32 ways × 64 bytes per set = 2 KB per set) would require a different approach.

---





## 10. External Resources and AI Usage

During Task 3, we used **Claude Sonnet 4.6**. Below lists some of our usage of AI during the full optimization process, including mistakes and how AI helped resolve them. We can't list all of the questions since we would like to talk about what we gained from using AI, in detail.



### Bug — SRRIP `getVictim` Recursive Stack Overflow

Our first SRRIP implementation used recursion for the age-and-retry loop:

```cpp
// FIRST (buggy) version
int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    for (int i = 0; i < (int)set.size(); ++i)
        if (set[i].rrpv == 3) return i;
    for (int i = 0; i < (int)set.size(); ++i)
        set[i].rrpv++;
    return getVictim(set); // recursive retry
}
```

It worked on simple tests but crashed on traces with sustained full-set misses. We did not immediately understand why — the logic seemed correct — so we brought the code to Copilot and asked: *"My SRRIP getVictim uses recursion — ages all lines then calls itself. Sometimes it crashes on longer traces. Is recursion the right pattern here?"* Copilot replied that *"the recursion is logically correct but structurally risky. In the worst case you need 3 full aging passes to reach RRPV=3 — that's fine. But if any bug prevents RRPV from reaching 3, the recursion is infinite. The standard implementation uses a `while(true)` loop: scan for RRPV==3, age all by +1 if none found, repeat. It terminates because RRPV is bounded at 3 and aging makes guaranteed progress. It also avoids all stack depth risk."* This made us understand that for retry-until-found patterns with bounded state, an explicit loop is always safer and more idiomatic than recursion.

Fixed version:

```cpp
int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    while (true) {
        for (int i = 0; i < (int)set.size(); ++i)
            if (set[i].rrpv == 3) return i;
        for (int i = 0; i < (int)set.size(); ++i)
            if (set[i].rrpv < 3) set[i].rrpv++;
    }
}
```

The `< 3` guard also prevents wrap-around. Crash eliminated.

---



### Key Breakthrough — Set 40 Thrashing Is a Geometric Problem

After settling at AMAT 2.98 with all policy variations exhausted at assoc=8, we were stuck. We had tried every combination of SRRIP, BIP, LRU with NextLine and Stride, but nothing broke past roughly 2.80. Not understanding why more sophisticated policies weren't helping, we pasted the full trace analyzer output into Claude and asked: *"Set 40 has 21.53% of all L1 accesses. In windows 3072–4096 there are 20 unique blocks but only 1 set. With assoc=8, what is structurally happening? Why can't SRRIP or BIP fix it?"* Claude replied that *"this is conflict-miss thrashing, not a policy problem. You have 20 blocks mapping to set 40, but the set only has 8 ways. At any moment, only 8 of the 20 can be in cache. The moment block #9 is needed, one of the 8 active blocks is evicted — and that block is almost certainly needed again soon in the same loop. SRRIP helps when some blocks are scan polluters that will never be reused. But here all 20 blocks are reused repeatedly — there is no polluter. BIP similarly cannot help because the eviction is forced by the working set exceeding the set capacity, not by poor ordering. The only structural fix is increasing associativity. Since `TASK3_ASSOC` is configurable in the Makefile, raising from 8 to 32 is the most direct lever."*

**This was the breakthrough.** We had been thinking about which policy to use, when the actual problem was geometric — the set physically could not hold the hot working set. We immediately tested:

```
assoc=16, LRU + NextLine + L2  →  AMAT: 2.45
assoc=32, LRU + NextLine + L2  →  AMAT: 1.89  ✓
```

---



### LRU vs SRRIP at High Associativity

With assoc=32 achieving 99.05% L1 hit rate, we still wanted to understand whether SRRIP could squeeze out any further improvement for the scan-heavy phases. We asked Claude: *"At assoc=32 with LRU + NextLine I get 99.05% L1 hit rate and AMAT 1.89. Should I still try SRRIP, and why might it not help?"* Claude reasoned that *"with assoc=32, the thrashing bottleneck is resolved — all 20 hot blocks fit comfortably. The scan phases touch 512 unique blocks per window with no intra-window reuse, so any policy evicts blocks that won't return anyway. SRRIP's insert-at-RRPV=2 shines when a scan polluter displaces a hot residual block, but if the working set fits and the prefetcher covers sequential regions, LRU's simple recency ordering is already optimal. The aging overhead in SRRIP adds work without benefit at this associativity."* This made us understand that the value of an advanced replacement policy is highly context-dependent: SRRIP helps when there is a genuine mix of hot and cold blocks competing for the same ways, but not when the working set fits or when every block in a scan is equally disposable. We confirmed this experimentally — SRRIP+NextLine at assoc=32 gave 1.90 cycles, and LRU+NextLine gave 1.89.

---







