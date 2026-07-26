# Distributed Cache

A compact Java 21 distributed in-memory cache with bounded LRU storage,
replication, failure injection, and four-node benchmarking.

## Design

- Four Dockerized nodes expose a small HTTP API and own independent caches.
- Java's access-ordered `LinkedHashMap` gives average O(1) `GET`, `PUT`,
  `DELETE`, and LRU promotion/eviction. Synchronized cache operations make it
  safe for concurrent virtual-thread requests.
- The client uses rendezvous hashing, which minimizes remapping when nodes change.
- Writes go to two owners. Reads try the primary then its replica, so one failed
  node does not interrupt access to already replicated keys.

This is an availability-oriented cache, not a consensus database: concurrent
writes during a partition can diverge and restarts intentionally lose memory.

## Run

Requires Docker Compose and Python 3.10+. Java is installed inside the Docker
image, so a local JDK is optional.

```sh
docker build -t distributed-cache:local .
docker compose up -d --no-build --wait
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

The Java unit test is compiled and executed automatically while the Docker image
is built:

```sh
docker build -t distributed-cache:local .
python tests/node_failure_test.py
python benchmarks/benchmark.py --requests 20000 --clients 32
```

To run Java tests locally when JDK 21 is installed:

```sh
mkdir -p out/main out/test
javac --release 21 -d out/main src/main/java/com/distributedcache/*.java
javac --release 21 -cp out/main -d out/test src/test/java/com/distributedcache/*.java
java -ea -cp out/main:out/test com.distributedcache.LruCacheTest
```

The failure test starts the cluster, writes a replicated key, stops that key's
actual primary, verifies replica service, and restarts the node. The benchmark
reports throughput, average/p50/p95/p99 latency, and per-node statistics.

```sh
docker compose down
```
