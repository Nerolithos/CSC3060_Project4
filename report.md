# Project 4 Report

**Members: 124090960, 124090302.**



# Problems and Findings

## Task 1

**1. Address subdivision**

```cpp
// memory_heirarchy.cpp
uint64_t without_offset = addr >> offset_bits;
uint64_t index_mask = (1ULL << index_bits) - 1ULL;
return without_offset & index_mask;
```

In general, num_sets in a cache is **usually a power of two**, so using `(addr >> offset_bits) % num_sets` to compute the index would also be completely correct. Still, I personally prefer the masking form: `index = (addr >> offset_bits) & ((1 << index_bits) - 1)`, because it feels a bit safer.

</br>

**2. Validity and dirty bit**

```cpp
void CacheLevel::write_back_victim(const CacheLine& line, uint64_t index, uint64_t cycle) {
	if (!line.dirty || !line.valid) return;
	// ......
```

Strictly speaking, even if I only checked dirty and did not check valid, it would still be fine. This is because Project 4 only simulates a **single-processor** system and **dosn't involve cache coherence** across multiple cores. So in this project, a case like “`dirty == true && valid == false`” should never actually happen. The extra valid check is just for defense. From this point and the previous one, you can probably tell that I am a bit obsessed with robustness and compatibility.

</br>

**3. Choice of “timestamp” in the LRU policy**

When implementing LRU in repl_policy.cpp, my first idea was to use a globally increasing counter as the timestamp, but I was not sure where to place it. Later I realized that the access function already takes a cycle parameter, and since the whole simulation processes the trace one instruction at a time in order, **using cycle directly as the timestamp is both simple and reasonable**.

That said, I did make a small bug at first. In getVictim, I did not consider that **some lines might still have the default value 0 in last_access**. As a result, a newly inserted line could incorrectly look “older” than a line that had never really been accessed. While debugging, I checked the fields of each line in the set and realized that I should consistently update last_access to the current cycle both on insertion and on a hit. This way, **0 would only remain on lines that had never been used** and were already marked `valid = false`, so it wouldn't interfere with the LRU decision.

</br>

</br>

## Task 2

Task 2 was relatively easy after we understood how the cache hierarchy should work.

The target structure is L1 -> L2 -> Main Memory, so we just need to insert L2 as the new lower level of L1. According to the project requirement, L2 should only be enabled when "enable-l2" is entered, and its role is to serve some L1 misses before the request goes to main memory. 

Based on this understanding, we implemented Task 2 mainly in main.cpp. We first created L2 with the given configuration, and finally redirected L1’s next level from memory to L2. This means the access path changed from L1 -> Main Memory in Task 1 to L1 -> L2 -> Main Memory in Task 2. Since the miss path in memory_hierarchy.cpp was already recursive, we did not need to redesign the whole access logic. Once L1 was connected to L2, the hierarchy worked naturally.

</br>

</br>

## Task 3

### Overview

We Optimize AMAT on the trace generated from student ID seed **`124090960`**. The trace was produced to `my_trace.txt`.

The target was to push **Student_AMAT below Best_AMAT** (**1.9 cycles** in Part3_best_AMAT.xsl) for full marks. We started from a **Baseline_AMAT of 24.43 cycles** (L1 + L2, LRU, no prefetch, assoc.=8) and improved step by step to **1.89 cycles**, surpassing the full-score threshold.



### Step 0: Implementations Added for Task 3

Before running any experiments, we implemented the following components that the starter code left as TODO:

#### `repl_policy.cpp` — SRRIP and BIP

**SRRIP (Static Re-Reference Interval Prediction):**

SRRIP uses a 2-bit RRPV (Re-Reference Prediction Value) per cache line. The design logic is:
- **On insertion (miss):** set RRPV = 2 (long re-reference, not yet proven useful).
- **On hit:** promote to RRPV = 0 (near-immediate re-reference, likely to be used again soon).
- **On eviction (`getVictim`):** evict the line with RRPV = 3. If none exists, age all lines by +1 and retry.

This design prevents a hot line from being evicted immediately after insertion and resists scan-heavy pollution by aging out lines that are never hit.

```cpp
void SRRIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    set[way].rrpv = 2; // insert with long re-reference interval
}
void SRRIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    set[way].rrpv = 0; // promote to near-immediate
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

**BIP (Bimodal Insertion Policy):**

BIP is a variant of LRU that inserts most new lines at the **LRU position** (setting `last_access = 0`) rather than the MRU position. This prevents a streaming scan from evicting the entire working set. Every `throttle`-th miss is allowed to insert at MRU, preserving some recency adaptivity.

```cpp
void BIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    insertion_counter++;
    if (insertion_counter % throttle == 0)
        set[way].last_access = cycle; // occasional MRU
    else
        set[way].last_access = 0;    // biased toward LRU position
}
```

#### `prefetcher.cpp` — NextLine and Stride

**NextLine Prefetcher:**  
On every access (hit or miss), it prefetches the immediately following cache block. This exploits the dominant stride-1 pattern in the trace.

```cpp
std::vector<uint64_t> NextLinePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    uint64_t block_base = (current_addr / block_size) * block_size;
    return {block_base + block_size};
}
```

**Stride Prefetcher:**  
Tracks the stride between consecutive block addresses. Once the same stride has appeared `CONF_THRESHOLD` (= 2) times in a row, it issues a prefetch one stride ahead.

#### `memory_hierarchy.cpp` — `install_prefetch`

We also completed `install_prefetch(addr, cycle)` in `memory_hierarchy.cpp`. This function installs a prefetched block into the cache without counting it as a demand miss. Internally it reuses the existing miss-path logic: look for an invalid slot first, then fall back to `getVictim`, perform a write-back if dirty, and mark the new line valid and clean. This ensures prefetched lines enter the replacement policy's metadata correctly.



### Step 1: Trace Analysis

We ran the trace analyzer to understand the access pattern before choosing any policy:

```bash
python3 trace_analyzer.py my_trace.txt --block-size 64 --window-size 512 --top 10
```

**Key findings:**

| Metric | Value |
|---|---|
| Total accesses | 5,992 |
| Read ratio | 96.86% (5,804 reads) |
| Write ratio | 3.14% (188 writes) |
| Unique blocks touched | 1,236 |
| Dominant stride | +1 block (60.91% of all transitions) |
| Second stride | +64 blocks (23.94%) |

**Hot sets:**

| L1 Set | Accesses | % of Total |
|---|---|---|
| Set 40 | 1,290 | 21.53% |
| Set 1 | 545 | 9.10% |
| Set 26 | 318 | 5.31% |
| Sets 38–45 | ~90 each | 1.50% each |

**Per-window phase analysis:**

| Window | Reads | Writes | Unique Blocks | Unique Sets | Phase Type |
|---|---|---|---|---|---|
| 0 – 512 | 470 | 42 | 192 | 64 | Mixed access |
| 512 – 1024 | 482 | 30 | 512 | 64 | Large scan |
| 1024 – 1536 | 512 | 0 | 512 | 64 | Pure scan |
| 1536 – 2048 | 512 | 0 | 512 | 64 | Pure scan |
| 2048 – 2560 | 512 | 0 | 247 | 64 | Partial reuse |
| 2560 – 3072 | 508 | 4 | 34 | 2 | Small locality |
| 3072 – 3584 | 487 | 25 | 20 | 1 | Hot loop (set 40!) |
| 3584 – 4096 | 486 | 26 | 20 | 1 | Hot loop (set 40!) |
| 4096+ | 488–498 | 14–24 | 320–451 | 64 | Large scan resumes |

**Interpretation:**

1. **Stride-1 dominance (60.91%)** → the trace is strongly sequential. NextLine prefetching has a high chance of being effective because the next block is almost always the next sequential one.
2. **Scan phases (windows 1024–2048):** 512 unique blocks per window, touching all 64 sets. This is a classic streaming scan where LRU will suffer because old useful blocks are evicted by the scan.  
3. **Hot loop phase (windows 3072–4096):** only 20 unique blocks residing entirely in set 40. With assoc=8, set 40 has only 8 ways but needs to hold 20 active blocks — this is **direct set thrashing**. Increasing associativity is the key fix.
4. **Stride-64 (23.94%)** is likely caused by the loop jumping across a large array in 64-block strides. A Stride prefetcher could help, but since stride-1 is far more dominant and NextLine is simpler, NextLine is the better starting prefetcher.





### Step 2: AI-Assisted Optimization — Journey, Bugs, and Breakthroughs

During Task 3, I used **GitHub Copilot (Claude Sonnet 4.6)** as a reasoning partner at each stuck point. Below is an honest account of the full optimization journey, including the mistakes I made and how AI helped me understand them.

---

#### 2.1 The Starting Point: AMAT 2.98 — Why Wasn't It Better?

After completing the SRRIP, BIP, NextLine, Stride, and `install_prefetch` implementations, I ran what I thought was a reasonable starting configuration:

```
assoc=8, SRRIP + NextLine, --enable-l2 SRRIP None  →  AMAT: 2.98 cycles
```

I expected this to be quite good — SRRIP is designed for scan resistance, NextLine should help sequential access, and L2 helps with misses. But 2.98 still felt far from the 1.9 target. I wasn't sure what the remaining bottleneck was. My first instinct was to try different policies.

I ran several combinations at assoc=8:
- BIP + NextLine → 2.80
- LRU + None → 24.43
- BIP + None → similar to BIP+NextLine

None of these moved the needle meaningfully. At this point I decided to bring in AI to help me think structurally rather than blindly trying combinations.

---

#### 2.2 Bug #1: Prefetching Without L2 Made AMAT *Worse*

Early on, I tried adding NextLine prefetch **without enabling L2**, expecting it to help since the trace is 60% stride-1:

```
assoc=8, LRU + NextLine, no L2  →  AMAT: 27.77 cycles  (worse than baseline 24.43!)
```

This was completely counterintuitive. I added prefetching and performance got *worse*? I asked AI to explain:

> **Me:** *"I added NextLine prefetching to my L1 cache but didn't enable L2. The baseline without prefetching is AMAT 24.43 cycles, but with NextLine it jumped to 27.77 cycles. The L1 hit rate did improve from 30% to 73%, so why is AMAT worse?"*
>
> **Copilot:** *"The hit rate improvement is real, but look at what's happening on the miss side. Your prefetcher issued 4,199 prefetch requests. Each of those prefetch misses goes directly to main memory at 100 cycles each. Even though demand hits improved, the total cycle count exploded because you added thousands of expensive memory accesses that didn't exist before. The AMAT formula accumulates all accesses including prefetch installs — and each of those 4,199 main-memory fills costs 100 cycles. Without L2 acting as a 4-cycle buffer, the prefetcher is essentially scheduling thousands of 100-cycle penalties. With L2, that same prefetch fill costs only 4 cycles, which changes the math entirely. The lesson: a prefetcher needs a fast lower level to absorb its misses. Without L2, aggressive prefetching is actively harmful."*

**What I learned:** Prefetching and L2 are a package deal. The prefetcher's value comes from converting 100-cycle main-memory hits into 1-cycle L1 hits *on subsequent demand accesses*, but only if the prefetch fills themselves are cheap. Without L2, each prefetch fill costs as much as a demand miss. This explained why my improvement path had to be: **first enable L2, then add prefetching**.

After I added L2, the same NextLine prefetch combination dropped from 27.77 to 2.98 cycles — a 9× improvement.

---

#### 2.3 Bug #2: SRRIP `getVictim` — Potential Infinite Loop

When I first wrote the SRRIP `getVictim` function, my implementation looked like this:

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

I used recursion for the "age and retry" loop. The code looked natural to me, but when I ran it under some test configurations, I occasionally got a stack overflow on traces with many consecutive full-set misses. I asked AI to review it:

> **Me:** *"My SRRIP getVictim uses recursion — it ages all lines then calls itself again. It seems to work most of the time but sometimes causes a crash on longer traces. Is recursion the right pattern here?"*
>
> **Copilot:** *"The recursion is logically correct but structurally risky. In the worst case — for example, a fully-associative set where every line was just inserted with RRPV=2 and none has ever been hit — you need 3 full aging passes to reach RRPV=3. That's 3 recursive calls, which is fine. But if you ever have a bug where RRPV values fail to reach 3 (e.g., due to an off-by-one in the aging logic), the recursion becomes infinite. The standard SRRIP implementation uses a `while(true)` loop: scan for RRPV==3, if not found age all by +1, repeat. The loop terminates because RRPV values are bounded at 3 and aging always makes progress. It also avoids any stack depth risk entirely."*

Replaced the recursive version with an iterative `while(true)` loop:

```cpp
// FIXED version
int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    while (true) {
        for (int i = 0; i < (int)set.size(); ++i)
            if (set[i].rrpv == 3) return i;
        for (int i = 0; i < (int)set.size(); ++i)
            if (set[i].rrpv < 3) set[i].rrpv++;
    }
}
```

The guard `if (set[i].rrpv < 3)` also prevents an accidental wrap-around if `rrpv` were ever at max. The crash disappeared and correctness was maintained.

---

#### 2.4 Key Insight: Set Thrashing Was the Real Bottleneck

After fixing the bugs above and settling at AMAT 2.98, I was still stuck. I had tried SRRIP, BIP, LRU, NextLine, Stride — nothing broke past ~2.8 at assoc=8. I went back to the trace analyzer output and pasted it to AI:

> **Me:** *"The trace analyzer shows set 40 has 21.53% of all L1 accesses. In windows 3072–4096 there are 20 unique blocks but only 1 set in use. With assoc=8, what is happening structurally? Why can't SRRIP or BIP fix this?"*
>
> **Copilot:** *"This is conflict-miss thrashing, not a policy problem. Here's why no replacement policy can fix it: you have 20 distinct blocks all mapping to set 40, but the set only has 8 ways. At any given moment, only 8 of those 20 can be in cache. The moment block #9 is needed, one of the 8 active blocks must be evicted — and that evicted block will almost certainly be needed again in the same hot loop. SRRIP promotes hits to RRPV=0 and inserts at RRPV=2, which helps when some blocks are "scan polluters" that will never be reused. But here, ALL 20 blocks are reused repeatedly. There is no polluter to deprioritize. BIP's LRU-insertion strategy similarly has no effect because the eviction is forced by the working set size exceeding the set capacity — not by poor priority ordering. The only structural fixes are: (1) increase associativity so the set can hold all 20 blocks, (2) add a victim cache above the set, or (3) use software prefetch/reorganization to reduce aliasing. Since `TASK3_ASSOC` is configurable in Makefile, option 1 is by far the fastest fix."*

**This was the breakthrough.** I had been thinking about *which policy* to use, when the actual problem was *geometric* — the set simply wasn't deep enough to hold the hot working set. No policy can conjure extra ways. I immediately tested:

```
assoc=16, LRU + NextLine + L2  →  AMAT: 2.45 cycles
assoc=32, LRU + NextLine + L2  →  AMAT: 1.89 cycles
```

At assoc=32, all 20 hot blocks fit in set 40 (with 12 spare ways), and the hit rate jumped to 99.05%.

---

#### 2.5 AI Interaction — Choosing LRU vs SRRIP at High Associativity

Once I saw the assoc=32 result, I wanted to understand why LRU beat SRRIP at that point:

> **Me:** *"At assoc=8, SRRIP helped over LRU for scan-heavy phases. But at assoc=32 with LRU + NextLine I get 99.05% L1 hit rate and AMAT 1.89. Should I still try SRRIP at assoc=32, and why might it not help?"*
>
> **Copilot:** *"With assoc=32, the set-thrashing bottleneck (set 40) is resolved — all 20 hot blocks fit with room to spare. The scan-heavy phases (windows 1024–2048) touch 512 unique blocks per window with no intra-window reuse, so any replacement policy simply evicts blocks that won't come back anyway. SRRIP's insert-at-RRPV=2 strategy shines when a scan polluter displaces a hot residual block, but if the working set fits and the prefetcher covers sequential regions, LRU's recency ordering is already optimal. The aging overhead in SRRIP (scanning all ways to find RRPV=3, then aging all lines) adds work without benefit when the cache is well-sized. Measure it to be sure, but I'd predict LRU+NextLine at assoc=32 is within noise of SRRIP+NextLine at assoc=32."*

This matched the experimental data: SRRIP+NextLine at assoc=32 produced AMAT 1.90 cycles vs LRU+NextLine at 1.89 — essentially equal, with LRU slightly better due to simpler consistent promotion.

---

#### 2.6 Summary of AI-Assisted Insights

| Problem | My Initial Assumption | AI Correction | Outcome |
|---|---|---|---|
| Prefetch worsened AMAT (27.77) | Prefetch always helps if hit rate rises | Prefetch fills cost 100 cycles without L2 to absorb them | Enabled L2 first; AMAT dropped to 2.98 |
| SRRIP/BIP couldn't break 2.8 | More sophisticated policy = lower AMAT | Problem was structural (set capacity), not policy | Raised assoc to 32; AMAT dropped to 1.89 |
| SRRIP `getVictim` crashed | Recursive retry is fine for small depth | Recursion risks stack overflow; use iterative `while(true)` | Replaced with loop; crash eliminated |
| LRU vs SRRIP at assoc=32 | SRRIP should still help for scan phases | At high assoc+prefetch, LRU is already optimal | Confirmed experimentally; LRU=1.89, SRRIP≈1.90 |





### Step 3: Iterative Experiments

We ran the following configurations systematically and recorded each result:

| Config | L1 Assoc | L1 Policy | L1 Prefetch | L2 | L1 Hit % | AMAT |
|---|---|---|---|---|---|---|
| Baseline | 8 | LRU | None | LRU+None | 30.02% | 24.43 |
| +Prefetch only (no L2) | 8 | LRU | NextLine | — | 73.23% | 27.77 |
| +L2 + SRRIP + Prefetch | 8 | SRRIP | NextLine | SRRIP+None | 74.32% | 2.98 |
| assoc=16 + LRU + NextLine | 16 | LRU | NextLine | LRU+None | 85.10% | 2.45 |
| **assoc=32 + LRU + NextLine** | **32** | **LRU** | **NextLine** | **LRU+None** | **99.05%** | **1.89** |

**Note on the "no L2 + NextLine" result (AMAT 27.77):** Adding a prefetcher without L2 actually made things *worse*. Prefetching issues more main-memory accesses (4,199 prefetches × 100-cycle latency). Without L2 to absorb those prefetch misses, total cycles explode. This shows that **prefetching and L2 must work together**: L2 catches both demand misses and prefetcher misses cheaply (4 cycles), while the prefetcher ensures nearly all demand accesses become L1 hits.

**Why assoc=32 produced such a large jump (2.45 → 1.89):**  
Set 40 has 20 frequently-reused blocks. With assoc=16, 4 of those 20 blocks are always evicted; with assoc=32, all 20 fit with 12 ways to spare. The L1 hit rate jumped from 85.10% to 99.05% — a near-perfect cache for this trace's hot phase.





### Step 4: Final Configuration and Makefile

The final configuration written into Makefile:

```makefile
TASK3_ASSOC = 32
TASK3_BLOCK = 64
TASK3_L1_POLICY = LRU
TASK3_L1_PREFETCH = NextLine
TASK3_L2_POLICY = LRU
TASK3_L2_PREFETCH = None
```

Running `make task3` produces:

```
Constructed L2: 128KB, 32-way, 4cyc, [LRU + None]
Constructed L1: 32KB, 32-way, 1cyc, [LRU + NextLine]

=== Simulation Results ===
  [L1] Hit Rate: 99.05% (Access: 5992, Miss: 57, WB: 107)
       Prefetches Issued: 3059
  [L2] Hit Rate: 60.16% (Access: 3223, Miss: 1284, WB: 4)
       Prefetches Issued: 0
  [Main Memory] Total Accesses: 1288

Metrics:
  Total Instructions: 5992
  Total Cycles:       11320
  AMAT:               1.89 cycles
```

**Student_AMAT (1.89) < Best_AMAT (1.9) → Full score (50/50) for Task 3.**





### Step 5: Discussion — Why This Configuration Works

**LRU vs SRRIP at high associativity:**  
With 32 ways, LRU outperforms SRRIP for this trace. At high associativity the thrashing is gone, so LRU's simple recency ordering is optimal. SRRIP's insert-at-2 and RRPV aging add overhead without benefit when the hot working set fits comfortably.

**NextLine vs Stride:**  
The trace has stride-1 at 60.91%. NextLine always prefetches the next sequential block unconditionally, making it maximally aggressive on sequential patterns. Stride prefetcher requires observing the same stride twice before issuing a prefetch, which means it misses the first occurrence of every stride. For a mostly streaming trace, NextLine's simplicity is a strength. Stride did not outperform NextLine on this trace.

**L2 role:**  
L2 at 128 KB absorbs both demand misses carried past L1 and the prefetch-install traffic. Without L2, every prefetcher miss costs 100 cycles (main memory), making aggressive prefetching counterproductive. With L2 (4 cycles), prefetch-fill traffic is cheap and the net effect is positive.

**Residual misses:**  
The 57 remaining L1 misses (0.95%) are mostly cold-start misses at the beginning of each scan phase, where new unique blocks appear for the first time. These are compulsory misses that no policy can entirely eliminate.





### External Resources and AI Usage

We used **GitHub Copilot (Claude Sonnet 4.6)** throughout Task 3 as a primary reasoning partner. All interactions described in Step 2 above were conducted via GitHub Copilot in VS Code. The specific ways AI contributed are summarized here:

**1. Diagnosing counter-intuitive results:**  
When adding NextLine prefetch made AMAT *worse* (27.77 vs 24.43 baseline), I would not have understood why purely from the numbers — the L1 hit rate had clearly improved. Copilot's explanation of prefetch-fill cost against main memory was the missing piece that redirected me toward enabling L2 first.

**2. Code review for correctness:**  
The SRRIP recursive `getVictim` worked on simple tests but was fragile. I only suspected a problem because traces with sustained full-set misses caused a crash. Copilot identified the root cause (unbounded recursion depth risk) and provided the idiomatic iterative replacement pattern.

**3. Structural diagnosis of the bottleneck:**  
After exhausting policy combinations at assoc=8, I was running out of ideas. Copilot's analysis of the set-thrashing phenomenon — explaining why no replacement policy could fix a geometric capacity problem — was the single most valuable input of the entire optimization process. It redirected effort from policy tuning to associativity scaling, which produced the decisive 2.45 → 1.89 improvement.

**4. Forming and validating hypotheses:**  
Before running each new configuration, I used Copilot to predict the expected direction of change and why. This reduced blind trial-and-error and ensured the experiments were theory-driven. The predictions were accurate in all major cases.

**5. Implementation guidance for BIP:**  
For the BIP throttle parameter, I initially set `throttle = 32` (arbitrary). Copilot suggested that common implementations use values in the range 16–64 and that the exact value matters less than the general biasing toward LRU insertion, since the goal is probabilistic rather than precise. This helped me avoid over-engineering the parameter selection.

All AI interactions were conducted within the GitHub Copilot chat interface in VS Code. No external code was copied verbatim; all implementations were written and verified independently with AI used for explanation and review only.

