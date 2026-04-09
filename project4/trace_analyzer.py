#!/usr/bin/env python3

import argparse
from collections import Counter
from pathlib import Path


def parse_trace(path):
    accesses = []
    with open(path, "r", encoding="utf-8") as handle:
        for lineno, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 2:
                continue
            access_type, addr_text = parts
            if access_type not in ("r", "w"):
                continue
            try:
                addr = int(addr_text, 16)
            except ValueError as exc:
                raise ValueError(f"Invalid address on line {lineno}: {addr_text}") from exc
            accesses.append((access_type, addr))
    return accesses


def format_percent(numerator, denominator):
    if denominator == 0:
        return "N/A"
    return f"{(100.0 * numerator / denominator):.2f}%"


def print_counter(title, counter, total, limit):
    print(title)
    if not counter:
        print("  (none)")
        return
    for key, count in counter.most_common(limit):
        share = format_percent(count, total)
        print(f"  {key}: {count} ({share})")


def analyze_windows(accesses, block_size, window_size, num_sets):
    print(f"Per-window summary (window size = {window_size} accesses)")
    if not accesses:
        print("  (no accesses)")
        return

    for start in range(0, len(accesses), window_size):
        chunk = accesses[start : start + window_size]
        reads = sum(1 for access_type, _ in chunk if access_type == "r")
        writes = len(chunk) - reads
        blocks = [addr // block_size for _, addr in chunk]
        unique_blocks = len(set(blocks))

        summary = (
            f"  [{start:6d}, {start + len(chunk):6d}) "
            f"reads={reads:4d} writes={writes:4d} unique_blocks={unique_blocks:4d}"
        )
        if num_sets:
            unique_sets = len({block % num_sets for block in blocks})
            summary += f" unique_sets={unique_sets:4d}"
        print(summary)


def main():
    parser = argparse.ArgumentParser(
        description="Basic trace analysis helper for CSC3060 Project 4."
    )
    parser.add_argument("trace", help="Path to the trace file")
    parser.add_argument(
        "--block-size",
        type=int,
        default=64,
        help="Cache block size in bytes for block/stride analysis (default: 64)",
    )
    parser.add_argument(
        "--l1-kb",
        type=int,
        default=32,
        help="L1 cache size in KB for set-level summaries (default: 32)",
    )
    parser.add_argument(
        "--assoc",
        type=int,
        default=8,
        help="L1 associativity for set-level summaries (default: 8)",
    )
    parser.add_argument(
        "--window-size",
        type=int,
        default=256,
        help="Number of accesses per summary window (default: 256)",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=8,
        help="Number of top entries to print in each histogram (default: 8)",
    )
    args = parser.parse_args()

    if args.block_size <= 0 or args.l1_kb <= 0 or args.assoc <= 0 or args.window_size <= 0:
        raise SystemExit("All numeric arguments must be positive.")

    accesses = parse_trace(args.trace)
    total = len(accesses)
    reads = sum(1 for access_type, _ in accesses if access_type == "r")
    writes = total - reads

    blocks = [addr // args.block_size for _, addr in accesses]
    unique_blocks = len(set(blocks))
    min_addr = min((addr for _, addr in accesses), default=None)
    max_addr = max((addr for _, addr in accesses), default=None)

    l1_sets = (args.l1_kb * 1024) // (args.block_size * args.assoc)
    set_counter = Counter((block % l1_sets) for block in blocks) if l1_sets > 0 else Counter()

    stride_counter = Counter()
    for prev_block, curr_block in zip(blocks, blocks[1:]):
        delta = curr_block - prev_block
        if delta != 0:
            stride_counter[delta] += 1

    hot_block_counter = Counter(blocks)

    print("=== CSC3060 Basic Trace Analyzer ===")
    print(f"Trace:          {Path(args.trace)}")
    print(f"Total accesses: {total}")
    print(f"Reads:          {reads} ({format_percent(reads, total)})")
    print(f"Writes:         {writes} ({format_percent(writes, total)})")
    print(f"Unique blocks:  {unique_blocks}")
    if min_addr is not None:
        print(f"Address range:  0x{min_addr:x} .. 0x{max_addr:x}")
    print(f"Block size:     {args.block_size} B")
    print(f"L1 sets:        {l1_sets}")
    print()

    print_counter("Top block strides (in cache blocks)", stride_counter, max(total - 1, 1), args.top)
    print()
    print_counter("Most frequently accessed blocks", hot_block_counter, total, args.top)
    print()
    print_counter("Most frequently used L1 sets", set_counter, total, args.top)
    print()
    analyze_windows(accesses, args.block_size, args.window_size, l1_sets)
    print()
    print("Suggested next steps")
    print("  1. If one small stride dominates, test NextLine or Stride prefetching.")
    print("  2. If a few blocks or sets dominate, explain the hotspot and reuse behavior.")
    print("  3. If different windows look very different, discuss phase changes in your report.")


if __name__ == "__main__":
    main()
