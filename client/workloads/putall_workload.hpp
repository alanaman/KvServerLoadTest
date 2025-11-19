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
    PutAllWorkload() : dist(1, LARGE_KEYSPACE_END) {}

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int key = dist(gen);
        std::string path = "/key/" + std::to_string(key);
        std::string value = "value-" + std::to_string(key);
        return cli.Put(path.c_str(), value, "text/plain");
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<PutAllWorkload>(*this);
    }
};
