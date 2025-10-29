#pragma once

#include <optional> // For std::optional (requires C++17)
#include <cstddef>  // For size_t
#include <memory>   // For std::unique_ptr

/**
 * @brief Abstract interface for a thread-safe lookaside key-value cache.
 *
 * This class defines the contract for a key-value cache. All implementations
 * are expected to be thread-safe, meaning all public methods can be
 * called concurrently from multiple threads without external locking.
 *
 * @tparam KeyType The type of the key.
 * @tparam ValueType The type of the value to be stored.
 */
template <typename KeyType, typename ValueType>
class IKeyValueCache {
public:
    // Virtual destructor is essential for any base class
    virtual ~IKeyValueCache() = default;

    /**
     * @brief Inserts or updates a value associated with a key.
     *
     * @param key The key for the entry.
     * @param value The value to store.
     */
    virtual void Put(const KeyType& key, const ValueType& value) = 0;

    /**
     * @brief Retrieves a value by its key.
     *
     * In a lookaside pattern, the application calls this first.
     * If it returns std::nullopt (cache miss), the application must
     * fetch the data from the source (e.g., database) and then call Put().
     *
     * @note This method is non-const because a cache lookup often
     * modifies internal state (e.g., updating LRU counters).
     *
     * @param key The key to look up.
     * @return An std::optional containing the value if found (cache hit),
     * or std::nullopt if not found (cache miss).
     */
    virtual std::optional<ValueType> Get(const KeyType& key) = 0;

    /**
     * @brief Removes a key-value pair from the cache.
     *
     * @param key The key to remove.
     * @return true if an item was successfully removed, false if the
     * key was not found.
     */
    virtual bool Remove(const KeyType& key) = 0;

    /**
     * @brief Clears all entries from the cache.
     */
    virtual void Clear() = 0;

    /**
     * @brief Returns the current number of items in the cache.
     *
     * @return The number of items.
     */
    virtual size_t Size() const = 0;

protected:
    // Protected constructor to prevent direct instantiation
    IKeyValueCache() = default;

    // Disallow copy and move operations for the base class
    IKeyValueCache(const IKeyValueCache&) = delete;
    IKeyValueCache& operator=(const IKeyValueCache&) = delete;
    IKeyValueCache(IKeyValueCache&&) = delete;
    IKeyValueCache& operator=(IKeyValueCache&&) = delete;
};