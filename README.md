# Distributed Cache

A compact C++17 distributed in-memory cache inspired by
[`rabbicse/distributed-cache`](https://github.com/rabbicse/distributed-cache).
It completes the distributed portion of that project's roadmap and adds bounded
LRU storage, failure injection, and four-node benchmarking.

## Design

- Four Dockerized nodes expose a small HTTP API and own independent caches.
- A hash table plus recency list gives average O(1) `GET`, `PUT`, `DELETE`, and
  LRU promotion/eviction, protected by a mutex for concurrent requests.
- The client uses rendezvous hashing, which minimizes remapping when nodes change.
- Writes go to two owners. Reads try the primary then its replica, so one failed
  node does not interrupt access to already replicated keys.

This is an availability-oriented cache, not a consensus database: concurrent
writes during a partition can diverge and restarts intentionally lose memory.

## Run

Requires Docker Compose and Python 3.10+.

```sh
docker compose up -d --build --wait
curl http://localhost:8081/health
```

Nodes use host ports 8081-8084. Set `CACHE_CAPACITY` before startup to override
the default 10,000 entries per node. Direct API examples:

```sh
curl -X PUT --data 'hello' http://localhost:8081/cache/example
curl http://localhost:8081/cache/example
curl -X DELETE http://localhost:8081/cache/example
curl http://localhost:8081/stats
```

Applications should use the distributed client:

```python
from client.cluster import DistributedCache
cache = DistributedCache()
cache.put("user:42", "cached value")
print(cache.get("user:42"))
```

## Test and benchmark

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python tests/node_failure_test.py
python benchmarks/benchmark.py --requests 20000 --clients 32
```

The failure test starts the cluster, writes a replicated key, stops that key's
actual primary, verifies replica service, and restarts the node. The benchmark
reports throughput, average/p50/p95/p99 latency, and per-node statistics.

```sh
docker compose down
```
