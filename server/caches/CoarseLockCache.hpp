#pragma once

#include "KeyValueCache.hpp"
#include <unordered_map>
#include <list>
#include <mutex> // For std::mutex and std::lock_guard

/**
 * @brief A thread-safe, fixed-size LRU (Least Recently Used) cache.
 *
 * Implements the IKeyValueCache interface.
 */
template <typename KeyType, typename ValueType>
class CoarseLRUCache : public IKeyValueCache<KeyType, ValueType> {
public:
    /**
     * @brief Constructs the LRU cache with a maximum size.
     * @param max_size The maximum number of items to store.
     */
    explicit CoarseLRUCache(size_t max_size) : max_size_(max_size) {
        if (max_size_ == 0) {
            throw std::invalid_argument("Cache max size must be greater than 0");
        }
    }

    /**
     * @brief Inserts or updates a value. If the cache is full,
     * evicts the least recently used item.
     */
    void Put(const KeyType& key, const ValueType& value) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(key);

        if (it != cache_map_.end()) {
            // Key already exists, update value and move to front (MRU)
            it->second.first = value;
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second.second);
        } else {
            // Key is new, check for eviction
            if (cache_map_.size() >= max_size_) {
                // Evict least recently used (back of the list)
                KeyType lru_key = lru_list_.back();
                cache_map_.erase(lru_key);
                lru_list_.pop_back();
            }

            // Add the new item to the front (MRU)
            lru_list_.push_front(key);
            cache_map_[key] = {value, lru_list_.begin()};
        }
    }

    /**
     * @brief Retrieves a value. If found (cache hit), marks it as
     * most recently used and returns the value.
     */
    std::optional<ValueType> Get(const KeyType& key) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(key);

        if (it == cache_map_.end()) {
            // Cache Miss
            return std::nullopt;
        }

        // Cache Hit: Move the accessed item to the front (MRU)
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.second);
        return it->second.first;
    }

    /**
     * @brief Removes a key-value pair from the cache.
     */
    bool Remove(const KeyType& key) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }

        // Remove from both map and list
        lru_list_.erase(it->second.second);
        cache_map_.erase(it);
        return true;
    }

    /**
     * @brief Clears all entries from the cache.
     */
    void Clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_map_.clear();
        lru_list_.clear();
    }

    /**
     * @brief Returns the current number of items in the cache (thread-safe).
     */
    size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_map_.size();
    }

private:
    // The list stores keys in LRU order (front = MRU, back = LRU)
    std::list<KeyType> lru_list_;

    // The map stores the key, the value, and an iterator to the
    // key's position in the lru_list_ for O(1) removal.
    using ListIterator = typename std::list<KeyType>::iterator;
    std::unordered_map<KeyType, std::pair<ValueType, ListIterator>> cache_map_;

    size_t max_size_;
    
    // Mutex to protect all R/W operations
    mutable std::mutex mutex_;
};