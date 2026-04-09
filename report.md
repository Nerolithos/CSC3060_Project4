

你可以直接用这段 Prompt 生成 report fragments：

> 请生成目前阶段的一份“像人类大学生写出来的” 合理的“人类程序员会犯的”错误、难点、挫折分析。请务必包括一段“如何使用 AI 解决问题”的分析（遇到什么没法解决的错误或问题、问了AI什么问题、AI 回答什么、我如何恍然大悟解决困难）。





# Task 1

**一、address subdivision**

```cpp
// memory_heirarchy.cpp
uint64_t without_offset = addr >> offset_bits;
uint64_t index_mask = (1ULL << index_bits) - 1ULL;
return without_offset & index_mask;
```

因为一般来说缓存的 `num_sets` 都是 2 的幂，直接用 `(addr >> offset_bits) % num_sets` 当作 index 也完全不会错，但我还是喜欢掩码的写法：`index = (addr >> offset_bits) & ((1 << index_bits) - 1)`。



**二、validity and dity**

```cpp
void CacheLevel::write_back_victim(const CacheLine& line, uint64_t index, uint64_t cycle) {
	if (!line.dirty || !line.valid) return;
	// ......
```

如果不这么写，只判断 dirty 但不判断 valid，也是没有问题的。因为“dirty 为 true 且 valid 为 false” 这种组合，在这次项目来说不会出现，只是作为防御性检查。



**三、 LRU 策略里的“时间戳”选择**
 在实现 repl_policy.cpp 里的 LRU 时，我一开始想用一个全局递增计数器当时间戳，但没想好放在哪里。后来觉得既然 access 函数里已经有 `cycle` 这个参数，而且整个模拟是按 trace 顺序一条一条执行的，用 `cycle` 直接当时间戳既简单又合理。
 不过我也犯了一个小 bug：最开始在 `getVictim` 时没有考虑到有些 line 的 `last_access` 还保留着默认值 0，结果新插入的行反而可能比从未访问过的行“更旧”。调试时看了一下 set 里每个 line 的字段，意识到应该统一在插入和命中时都把 `last_access` 设为当前 `cycle`，这样 0 只会出现在完全没被使用过、但已经被标记 valid=false 的行里，不会干扰 LRU 选择。
