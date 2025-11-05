#pragma once

#include "workload.hpp"

/**
 * @brief Workload b: "get_all"
 * Performs random GET requests from the main keyspace.
 * Assumes keys either exist (e.g., from a previous 'put_all' run)
 * or are expected to fail (cache-miss workload).
 */
class GetAllWorkload : public IWorkload {
    std::uniform_int_distribution<> dist;
public:
    GetAllWorkload() : dist(1, KEYSPACE_SIZE) {}

    void prepare(httplib::Client& cli) override {
        std::cout << "   Preparing popular keys (1-" << KEYSPACE_SIZE <<")...\n";
        int prepared_count = 0;
        int error_count = 0;
        for (int key = 1; key <= KEYSPACE_SIZE; ++key) {
            std::string path = "/key/" + std::to_string(key);
            std::string value = "value-" + std::to_string(key);
            if (auto res = cli.Put(path.c_str(), value, "text/plain")) {
                if (res->status == 200) {
                    prepared_count++;
                } else {
                    error_count++;
                    std::cerr << "     Error preparing key " << key << ": HTTP " << res->status << " " << res->body << "\n";
                }
            } else {
                error_count++;
            }
        }
        std::cout << "    Prepared " << prepared_count << " / " << KEYSPACE_SIZE << " keys (" << error_count << " errors).\n";
    }

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int key = dist(gen);
        std::string path = "/key/" + std::to_string(key);
        return cli.Get(path.c_str());
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<GetAllWorkload>(*this);
    }
};
