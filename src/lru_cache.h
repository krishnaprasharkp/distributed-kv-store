// ============================================================
// Phase 3: LRU Cache with TTL, as a standalone class
// ============================================================
// This class replaces the plain std::unordered_map<string,string>
// from Phase 1/2. It combines TWO data structures:
//
//   1. std::list<Node>            - a doubly linked list, ordered
//                                    by recency (front = most
//                                    recently used, back = least
//                                    recently used)
//   2. std::unordered_map<key, iterator into that list>
//                                    - gives O(1) lookup of WHERE
//                                    a key sits in the list
//
// Every GET/SET moves the accessed node to the FRONT of the list
// (it's now the most recently used). When the cache is full and a
// NEW key is inserted, the node at the BACK of the list (least
// recently used) is evicted.
//
// TTL: each node also stores an expiry timestamp. GET checks if
// the key has expired and treats it as a miss if so (lazy
// expiration - we don't need a background thread for this).
// ============================================================

#pragma once
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>

class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    // Insert or update a key. ttlSeconds = 0 means "never expires".
    void set(const std::string& key, const std::string& value, long ttlSeconds = 0) {
        std::lock_guard<std::mutex> lock(mtx_);

        auto expiry = ttlSeconds > 0
            ? std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds)
            : std::chrono::steady_clock::time_point::max(); // "never" = far future

        auto it = index_.find(key);
        if (it != index_.end()) {
            // Key already exists: update its value/expiry, and move
            // it to the front (most recently used).
            it->second->value = value;
            it->second->expiry = expiry;
            touch(it->second);
            return;
        }

        // New key. If we're at capacity, evict the least recently
        // used entry (the one at the back of the list) first.
        if (order_.size() >= capacity_) {
            evictLRU();
        }

        // Insert new node at the FRONT of the list (most recent),
        // and record its position in the hashmap for O(1) lookup.
        order_.push_front(Node{key, value, expiry});
        index_[key] = order_.begin();
    }

    // Returns the value if present and not expired, otherwise nullopt.
    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx_);

        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;

        // Lazy expiration: if this key's TTL has passed, treat it
        // as a miss and remove it now instead of waiting for a
        // background sweep.
        if (isExpired(it->second)) {
            order_.erase(it->second);
            index_.erase(it);
            return std::nullopt;
        }

        touch(it->second); // accessed -> move to front (most recent)
        return it->second->value;
    }

    bool remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = index_.find(key);
        if (it == index_.end()) return false;
        order_.erase(it->second);
        index_.erase(it);
        return true;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return order_.size();
    }

private:
    struct Node {
        std::string key;
        std::string value;
        std::chrono::steady_clock::time_point expiry;
    };

    size_t capacity_;
    std::list<Node> order_;                                    // front = most recent, back = least recent
    std::unordered_map<std::string, std::list<Node>::iterator> index_; // key -> position in order_
    std::mutex mtx_;

    bool isExpired(const std::list<Node>::iterator& it) {
        return std::chrono::steady_clock::now() > it->expiry;
    }

    // Move a node to the front of the list. std::list::splice is
    // O(1) - it just relinks pointers, no copying of data.
    void touch(std::list<Node>::iterator it) {
        order_.splice(order_.begin(), order_, it);
    }

    void evictLRU() {
        if (order_.empty()) return;
        const std::string& lruKey = order_.back().key;
        index_.erase(lruKey);
        order_.pop_back();
    }
};
