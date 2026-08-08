
#pragma once
#include <vector>
#include <memory>
#include <functional>
#include "lru_cache.h"

class ShardedLRUCache {
public:
    ShardedLRUCache(size_t totalCapacity, size_t numShards = 16)
        : numShards_(numShards) {
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


    LRUCache& shardFor(const std::string& key) {
        size_t idx = hasher_(key) % numShards_;
        return *shards_[idx];
    }
};
