// ============================================================
// Phase 5: Sharded LRU Cache (reduces lock contention)
// ============================================================
// Problem this solves:
//   The single LRUCache (lru_cache.h) has ONE mutex protecting
//   the ENTIRE cache. Under many concurrent threads, they all
//   end up waiting on each other for that one lock, even when
//   touching completely unrelated keys. This caps how much
//   throughput multithreading can actually deliver.
//
// Fix:
//   Split one big cache into N independent smaller LRUCache
//   instances ("shards"), each with its OWN mutex (inherited
//   from LRUCache itself - each shard IS just a normal LRUCache).
//   Which shard a key belongs to is decided by hashing the key:
//       shardIndex = hash(key) % numShards
//   Two threads touching keys in DIFFERENT shards never wait on
//   each other at all - only threads touching the SAME shard can
//   block each other. With 16 shards, contention drops roughly
//   16x in the best case (uniformly distributed keys).
//
// Trade-off (important to understand, not just use):
//   Eviction is now PER-SHARD, not globally LRU. The "least
//   recently used key in the whole cache" concept doesn't exist
//   anymore - each shard independently evicts its own least
//   recently used key once IT is full. This is a real, accepted
//   trade-off in real systems (e.g. Redis Cluster) - you give up
//   perfectly precise global ordering in exchange for far less
//   lock contention. Total capacity is still respected (it's just
//   split evenly across shards instead of being one shared pool).
// ============================================================

#pragma once
#include <vector>
#include <memory>
#include <functional>
#include "lru_cache.h"

class ShardedLRUCache {
public:
    ShardedLRUCache(size_t totalCapacity, size_t numShards = 16)
        : numShards_(numShards) {
        // Each shard gets an equal slice of the total capacity.
        size_t perShardCapacity = std::max(size_t(1), totalCapacity / numShards);
        for (size_t i = 0; i < numShards_; i++) {
            shards_.push_back(std::make_unique<LRUCache>(perShardCapacity));
        }
    }

    void set(const std::string& key, const std::string& value, long ttlSeconds = 0) {
        shardFor(key).set(key, value, ttlSeconds);
    }

    std::optional<std::string> get(const std::string& key) {
        return shardFor(key).get(key);
    }

    bool remove(const std::string& key) {
        return shardFor(key).remove(key);
    }

    size_t size() {
        size_t total = 0;
        for (auto& shard : shards_) total += shard->size();
        return total;
    }

private:
    size_t numShards_;
    std::vector<std::unique_ptr<LRUCache>> shards_;
    std::hash<std::string> hasher_;

    // Picks which shard a key belongs to. std::hash gives a
    // pseudo-random but DETERMINISTIC number for the same string,
    // so the same key always maps to the same shard.
    LRUCache& shardFor(const std::string& key) {
        size_t idx = hasher_(key) % numShards_;
        return *shards_[idx];
    }
};