

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

    void set(const std::string& key, const std::string& value, long ttlSeconds = 0) {
        std::lock_guard<std::mutex> lock(mtx_);

        auto expiry = ttlSeconds > 0
            ? std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds)
            : std::chrono::steady_clock::time_point::max(); 

        auto it = index_.find(key);
        if (it != index_.end()) {
       
            it->second->value = value;
            it->second->expiry = expiry;
            touch(it->second);
            return;
        }

        if (order_.size() >= capacity_) {
            evictLRU();
        }

     
        order_.push_front(Node{key, value, expiry});
        index_[key] = order_.begin();
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx_);

        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;

        if (isExpired(it->second)) {
            order_.erase(it->second);
            index_.erase(it);
            return std::nullopt;
        }

        touch(it->second); 
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
    std::list<Node> order_;                                    
    std::unordered_map<std::string, std::list<Node>::iterator> index_; 
    std::mutex mtx_;

    bool isExpired(const std::list<Node>::iterator& it) {
        return std::chrono::steady_clock::now() > it->expiry;
    }

  
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
