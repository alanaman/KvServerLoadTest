#pragma once

#include <iostream>
#include <queue>
#include <memory>       // For std::unique_ptr
#include <mutex>        // For std::mutex, std::unique_lock
#include <condition_variable> // For std::condition_variable
#include <functional>   // For std::function
#include <stdexcept>    // For std::runtime_error
#include <utility>      // For std::move

// Forward declaration
template <typename ConnectionType>
class ConnectionPool;

/**
 * @brief A RAII wrapper for a pooled connection.
 *
 * When this object is destroyed (goes out of scope), it automatically
 * returns the connection to the pool.
 */
template <typename ConnectionType>
class PooledConnection {
public:
    // Destructor: returns the connection to the pool
    ~PooledConnection() {
        if (pool_ && connection_) {
            pool_->release(std::move(connection_));
        }
    }

    // Delete copy operations
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;

    // Enable move operations
    PooledConnection(PooledConnection&& other) noexcept
        : connection_(std::move(other.connection_)), pool_(other.pool_) {
        // Invalidate the other object so it doesn't release the connection
        other.pool_ = nullptr;
    }

    PooledConnection& operator=(PooledConnection&& other) noexcept {
        if (this != &other) {
            // Release our current connection if we have one
            if (pool_ && connection_) {
                pool_->release(std::move(connection_));
            }

            // Take ownership from the other object
            connection_ = std::move(other.connection_);
            pool_ = other.pool_;

            // Invalidate the other object
            other.pool_ = nullptr;
        }
        return *this;
    }

    // Access the underlying connection object
    ConnectionType* operator->() { return connection_.get(); }
    const ConnectionType* operator->() const { return connection_.get(); }
    ConnectionType& operator*() { return *connection_; }
    const ConnectionType& operator*() const { return *connection_; }

    // Check if the wrapper holds a valid connection
    bool is_valid() const { return connection_ != nullptr; }
    explicit operator bool() const { return is_valid(); }

private:
    // Only ConnectionPool can create a PooledConnection
    friend class ConnectionPool<ConnectionType>;

    PooledConnection(std::unique_ptr<ConnectionType> conn, ConnectionPool<ConnectionType>* pool)
        : connection_(std::move(conn)), pool_(pool) {}

    std::unique_ptr<ConnectionType> connection_;
    ConnectionPool<ConnectionType>* pool_;
};


/**
 * @brief A thread-safe, generic connection pool.
 */
template <typename ConnectionType>
class ConnectionPool {
public:
    // Type alias for the factory function
    using ConnectionFactory = std::function<std::unique_ptr<ConnectionType>()>;

    /**
     * @brief Constructs the connection pool.
     * @param max_size The maximum number of connections to allow (idle + in-use).
     * @param factory A function (e.g., a lambda) that creates a new connection.
     */
    ConnectionPool(size_t max_size, ConnectionFactory factory)
        : max_size_(max_size), factory_(std::move(factory)), total_connections_(0) {
        if (max_size == 0) {
            throw std::invalid_argument("Connection pool max size must be greater than 0");
        }
    }

    // Delete copy operations
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    /**
     * @brief Acquires a connection from the pool.
     *
     * If the pool is empty and max size hasn't been reached, a new connection
     * is created. If the pool is empty and max size *has* been reached,
     * this call will block until a connection is returned to the pool.
     *
     * @return A PooledConnection wrapper.
     */
    PooledConnection<ConnectionType> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until a connection is available or we can create a new one
        condition_.wait(lock, [this]() {
            return !idle_connections_.empty() || total_connections_ < max_size_;
        });

        if (!idle_connections_.empty()) {
            // Get an existing idle connection
            std::unique_ptr<ConnectionType> conn = std::move(idle_connections_.front());
            idle_connections_.pop();
            
            // Unlock before returning, as PooledConnection ctor is simple
            lock.unlock(); 
            return PooledConnection<ConnectionType>(std::move(conn), this);
        }

        if (total_connections_ < max_size_) {
            // Create a new connection
            total_connections_++;
            
            // Unlock before calling the factory, as it might block (e.g., network I/O)
            lock.unlock(); 
            
            std::unique_ptr<ConnectionType> new_conn;
            try {
                new_conn = factory_();
            } catch (...) {
                // If factory fails, decrement count and rethrow
                std::unique_lock<std::mutex> lock_on_fail(mutex_);
                total_connections_--;
                lock_on_fail.unlock();
                condition_.notify_all(); // Notify others they might be able to create
                throw; // Rethrow the exception
            }

            return PooledConnection<ConnectionType>(std::move(new_conn), this);
        }

        // This should not be reachable due to the wait condition, but handle defensively
        throw std::runtime_error("Failed to acquire connection");
    }

    /**
     * @brief Gets the number of currently idle connections.
     */
    size_t get_idle_count() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return idle_connections_.size();
    }

    /**
     * @brief Gets the total number of connections created (idle + in-use).
     */
    size_t get_total_count() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return total_connections_;
    }


private:
    friend class PooledConnection<ConnectionType>;

    /**
     * @brief Returns a connection to the pool.
     * Called by the PooledConnection destructor.
     */
    void release(std::unique_ptr<ConnectionType> conn) {
        std::unique_lock<std::mutex> lock(mutex_);
        idle_connections_.push(std::move(conn));
        
        // Unlock before notifying
        lock.unlock(); 
        condition_.notify_one(); // Wake up one waiting thread
    }

    size_t max_size_;
    size_t total_connections_;
    ConnectionFactory factory_;

    std::queue<std::unique_ptr<ConnectionType>> idle_connections_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};