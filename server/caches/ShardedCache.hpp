#pragma once

#include "KeyValueCache.hpp"
#include <unordered_map>
#include <stdexcept>
#include <list>
#include <mutex>
#include <vector>
#include <functional> // For std::hash
#include <numeric>    // For std::iota

/**
 * @brief A thread-safe, sharded LRU cache for high concurrency.
 *
 * Implements the IKeyValueCache interface by sharding the cache into
 * N smaller caches, each with its own mutex. This allows operations
 * on different keys (that hash to different shards) to run in parallel.
 */
template <typename KeyType, typename ValueType>
class FineLRUCache : public IKeyValueCache<KeyType, ValueType> {
private:
    // Define a single "shard". It's just the non-thread-safe
    // data structures from your original CoarseLRUCache.
    struct Shard {
        using ListIterator = typename std::list<KeyType>::iterator;
        std::list<KeyType> lru_list_;
        std::unordered_map<KeyType, std::pair<ValueType, ListIterator>> cache_map_;
        size_t max_shard_size_;
        mutable std::mutex mutex_;

        explicit Shard(size_t max_size) : max_shard_size_(max_size) {}
    };

public:
    /**
     * @brief Constructs the sharded LRU cache.
     * @param max_size Total maximum items across all shards.
     * @param shard_count The number of shards to split the cache into.
     * A power of 2 is often a good choice.
     */
    explicit FineLRUCache(size_t max_size, size_t shard_count = 32)
        : num_shards_(shard_count) {
        if (max_size == 0) {
            throw std::invalid_argument("Cache max size must be greater than 0");
        }
        if (num_shards_ == 0) {
            throw std::invalid_argument("Shard count must be greater than 0");
        }

        // Calculate size per shard, ensuring we distribute the remainder
        size_t base_size = max_size / num_shards_;
        size_t remainder = max_size % num_shards_;


        for (size_t i = 0; i < num_shards_; ++i) {
            // Add 1 to the first 'remainder' shards
            size_t shard_size = base_size + (i < remainder ? 1 : 0);
            // Ensure no shard has size 0 if max_size < num_shards_
            if (shard_size == 0 && max_size > 0) {
                 shard_size = 1;
            }
            if (shard_size > 0) {
                shards_.emplace_back(std::make_unique<Shard>(shard_size));
            }
        }
        // Adjust num_shards_ in case some were 0-sized and skipped
        num_shards_ = shards_.size();
        if (num_shards_ == 0) {
             throw std::runtime_error("Cache could not be initialized");
        }
    }

    void Put(const KeyType& key, const ValueType& value) override {
        Shard& shard = getShardForKey(key);
        std::lock_guard<std::mutex> lock(shard.mutex_);

        auto it = shard.cache_map_.find(key);
        if (it != shard.cache_map_.end()) {
            it->second.first = value;
            shard.lru_list_.splice(shard.lru_list_.begin(), shard.lru_list_, it->second.second);
        } else {
            if (shard.cache_map_.size() >= shard.max_shard_size_) {
                KeyType lru_key = shard.lru_list_.back();
                shard.cache_map_.erase(lru_key);
                shard.lru_list_.pop_back();
            }
            shard.lru_list_.push_front(key);
            shard.cache_map_[key] = {value, shard.lru_list_.begin()};
        }
    }

    std::optional<ValueType> Get(const KeyType& key) override {
        Shard& shard = getShardForKey(key);
        std::lock_guard<std::mutex> lock(shard.mutex_);

        auto it = shard.cache_map_.find(key);
        if (it == shard.cache_map_.end()) {
            return std::nullopt;
        }

        shard.lru_list_.splice(shard.lru_list_.begin(), shard.lru_list_, it->second.second);
        return it->second.first;
    }

    bool Remove(const KeyType& key) override {
        Shard& shard = getShardForKey(key);
        std::lock_guard<std::mutex> lock(shard.mutex_);

        auto it = shard.cache_map_.find(key);
        if (it == shard.cache_map_.end()) {
            return false;
        }

        shard.lru_list_.erase(it->second.second);
        shard.cache_map_.erase(it);
        return true;
    }

    /**
     * @brief Clears all entries from all shards.
     */
    void Clear() override {
        for (auto& shard : shards_) {
            std::lock_guard<std::mutex> lock(shard->mutex_);
            shard->cache_map_.clear();
            shard->lru_list_.clear();
        } 
    }

    /**
     * @brief Returns the total number of items across all shards.
     */
    size_t Size() const override {
        size_t total_size = 0;
        for (const auto& shard : shards_) {
            std::lock_guard<std::mutex> lock(shard->mutex_);
            total_size += shard->cache_map_.size();
        }
        return total_size;
    }

private:
    /**
     * @brief Get the specific shard for a given key.
     *
     * Requires std::hash<KeyType> to be defined.
     */
    Shard& getShardForKey(const KeyType& key) {
        return const_cast<Shard&>(static_cast<const FineLRUCache*>(this)->getShardForKey(key));
    }

    const Shard& getShardForKey(const KeyType& key) const {
        size_t hash = key_hasher_(key);
        // Add dereference *
        return *shards_[hash % num_shards_];
    }

    size_t num_shards_;
    std::vector<std::unique_ptr<Shard>> shards_;
    std::hash<KeyType> key_hasher_;
};