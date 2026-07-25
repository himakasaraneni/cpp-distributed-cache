#!/usr/bin/env python3
"""Mixed-workload benchmark across all four nodes."""
import argparse, random, statistics, sys, time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from client.cluster import DistributedCache
def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * fraction))]
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--requests", type=int, default=20_000)
    parser.add_argument("--clients", type=int, default=32)
    parser.add_argument("--keys", type=int, default=5_000)
    parser.add_argument("--write-ratio", type=float, default=.2)
    args = parser.parse_args()
    cache = DistributedCache(timeout=2)
    def operation(index):
        rng = random.Random(index)
        key = f"bench-{rng.randrange(args.keys)}"
        start = time.perf_counter_ns()
        cache.put(key, f"value-{index}") if rng.random() < args.write_ratio else cache.get(key)
        return (time.perf_counter_ns() - start) / 1_000_000
    started = time.perf_counter()
    with ThreadPoolExecutor(max_workers=args.clients) as pool:
        latencies = list(pool.map(operation, range(args.requests)))
    elapsed = time.perf_counter() - started
    print(f"nodes:       {len(cache.nodes)} (replication factor {cache.replicas})")
    print(f"requests:    {args.requests}\nclients:     {args.clients}")
    print(f"throughput:  {args.requests / elapsed:,.0f} ops/sec")
    print(f"latency avg: {statistics.mean(latencies):.3f} ms")
    for name, fraction in (("p50", .5), ("p95", .95), ("p99", .99)):
        print(f"latency {name}: {percentile(latencies, fraction):.3f} ms")
    print("node stats:")
    for stats in cache.stats(): print(f"  {stats}")
if __name__ == "__main__": main()
