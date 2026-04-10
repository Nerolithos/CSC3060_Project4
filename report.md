# **Task 1**

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

# **Task 2**

Task 2 was relatively easy after I understood how the cache hierarchy should work.

The target structure is L1 -> L2 -> Main Memory, so I just need to insert L2 as the new lower level of L1. According to the project requirement, L2 should only be enabled when "enable-l2" is entered, and its role is to serve some L1 misses before the request goes to main memory. 

Based on this understanding, I implemented Task 2 mainly in main.cpp. I first created L2 with the given configuration, and finally redirected L1’s next level from memory to L2. This means the access path changed from L1 -> Main Memory in Task 1 to L1 -> L2 -> Main Memory in Task 2. Since the miss path in memory_hierarchy.cpp was already recursive, I did not need to redesign the whole access logic. Once L1 was connected to L2, the hierarchy worked naturally.
</br>
