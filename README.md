# Distributed Key-Value Store (C++)

A multi-threaded, in-memory key-value store built from scratch in C++ — inspired by Redis. Built to demonstrate systems-level engineering: TCP networking, concurrency, thread-safe data structures, and performance benchmarking/optimization.

## Features

- **TCP server/client** — custom text-based protocol over raw sockets (`SET`, `GET`, `DEL`)
- **Multi-threaded** — one thread per client connection, handled concurrently
- **LRU eviction** — fixed-capacity cache; automatically evicts the least recently used key when full
- **TTL support** — keys can expire after N seconds (`SET key value EX 60`)
- **Sharded locking** — the cache is split into 16 independently-locked shards to reduce lock contention under concurrent load
- **Custom benchmarking harness** — measures real throughput (ops/sec) and latency percentiles (p50/p95/p99) under configurable thread counts

## Architecture

```
Client(s) <--TCP--> Server (accepts + spawns 1 thread/client)
                          |
                          v
                  ShardedLRUCache
                  /       |       \
              Shard 0   Shard 1  ... Shard 15
             (LRUCache) (LRUCache)   (LRUCache)
             own mutex   own mutex    own mutex
```

Each shard is an independent `LRUCache` (hashmap + doubly linked list for O(1) get/set/evict), with its own mutex. A key's shard is chosen via `hash(key) % 16`, so unrelated keys rarely contend for the same lock.

## Build & Run

Requires a C++17 compiler and `make` (Linux/WSL recommended — uses POSIX sockets).

```bash
make
./server        # starts the server on port 6380
./client         # in a separate terminal, interactive client
```

Example session:
```
> SET name Krishna
OK
> GET name
Krishna
> SET session1 abc123 EX 60
OK
> DEL name
OK
```

## Benchmarking

```bash
./benchmark <num_threads> <requests_per_thread>
# e.g.
./benchmark 8 5000
```

Outputs throughput (ops/sec) and p50/p95/p99 latency. See `benchmark_results.txt` for measured results across 1/2/4/8 threads.

## Project Structure

```
src/
  server.cpp        - TCP server, connection handling, command parsing
  client.cpp         - interactive TCP client
  lru_cache.h         - thread-safe LRU cache (hashmap + doubly linked list)
  sharded_cache.h     - splits the cache into N independently-locked shards
  benchmark.cpp       - multi-threaded load generator + latency measurement
Makefile
```

## Design Notes / Trade-offs

- **TTL is lazily evaluated** — expired keys are removed on next access, not via a background sweep (simpler, no extra thread, standard approach for a project at this scale).
- **Sharded eviction is per-shard, not globally LRU** — splitting the cache into shards means "least recently used" is tracked independently per shard, not across the whole cache. This trades small precision loss for significantly reduced lock contention — the same trade-off real distributed caches (e.g. Redis Cluster) make.

## What This Project Demonstrates

TCP/socket programming · multithreading (`std::thread`, `std::mutex`) · thread-safe data structure design · lock contention and sharding as a mitigation · performance benchmarking methodology (throughput vs. latency, percentiles vs. averages)
