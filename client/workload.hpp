#pragma once

#include "httplib.h"
#include "workload_defs.hpp"

/**
 * @brief Abstract interface for a workload operation.
 *
 * Each thread will receive its own clone of a workload object,
 * allowing it to maintain its own state (like random distributions)
 * without needing locks.
 */
class IWorkload {
public:
    virtual ~IWorkload() = default;

    /**
     * @brief (Optional) Pre-populates the server with required data
     * before the test starts.
     * @param cli A client for executing preparation requests.
     */
    virtual void prepare(httplib::Client& cli) {
        // Default implementation does nothing.
    }

    /**
     * @brief Executes a single workload operation (e.g., one HTTP request).
     * @param cli The HTTP client dedicated to this thread.
     * @param gen The random number generator dedicated to this thread.
     * @return The result of the HTTP operation.
     */
    virtual httplib::Result execute(httplib::Client& cli, std::mt19937& gen) = 0;

    /**
     * @brief Creates a deep copy of the workload object.
     * @return A std::unique_ptr to the new IWorkload instance.
     */
    virtual std::unique_ptr<IWorkload> clone() const = 0;
};