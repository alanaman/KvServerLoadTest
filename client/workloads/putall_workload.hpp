#pragma once

#include "httplib.h"
#include "workload.hpp"


/**
 * @brief Workload a: "put_all"
 * Performs random PUT requests within the main keyspace.
 * No preparation needed as it creates its own keys.
 */
class PutAllWorkload : public IWorkload {
    std::uniform_int_distribution<> dist;
public:
    PutAllWorkload() : dist(1, KEYSPACE_SIZE) {}

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int key = dist(gen);
        std::string path = "/" + std::to_string(key);
        // Generate a much larger value to increase I/O pressure.
        // A ~4KB value is a good starting point as it's often the size of a memory page.
        // std::string large_string(4000, 'x');
        std::string value = "value-" + std::to_string(key);
        return cli.Put(path.c_str(), value, "text/plain");
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<PutAllWorkload>(*this);
    }
};
