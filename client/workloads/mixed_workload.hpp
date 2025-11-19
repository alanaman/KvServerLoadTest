#pragma once
#include "workload.hpp"


/**
 * @brief Workload d: "mixed"
 * 80% "get_popular" requests, 20% "unique_put" requests.
 * Pre-populates the popular keys in the prepare() step.
 */
class MixedWorkload : public IWorkload {
    std::uniform_int_distribution<> popular_dist;
    std::uniform_int_distribution<> mix_dist;
    std::uniform_int_distribution<> unique_write_dist;
public:
    MixedWorkload() :
        popular_dist(1, 100),
        mix_dist(0, 99),
        unique_write_dist(LARGE_KEYSPACE_START, LARGE_KEYSPACE_END) {}

    void prepare(httplib::Client& cli) override {
        // std::cout << "   Preparing popular keys (1-100) for 'mixed' workload...\n";
        // int prepared_count = 0;
        // int error_count = 0;
        // for (int key = 1; key <= 100; ++key) {
        //     std::string path = "/key/" + std::to_string(key);
        //     std::string value = "value-" + std::to_string(key);
        //     if (auto res = cli.Put(path.c_str(), value, "text/plain")) {
        //         if (res->status == 200) {
        //             prepared_count++;
        //         } else {
        //             error_count++;
        //         }
        //     } else {
        //         error_count++;
        //     }
        // }
        // std::cout << "   ... prepared " << prepared_count << " / 100 keys ("
        //           << error_count << " errors).\n";
    }

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int op = mix_dist(gen); // 0-99

        if (op < 80) { // 80% chance: Get popular
            int key = popular_dist(gen);
            std::string path = "/key/" + std::to_string(key);
            return cli.Get(path.c_str());
        } else { // 20% chance: Put "unique" (random from large space)
            int key = unique_write_dist(gen);
            std::string path = "/key/" + std::to_string(key);
            std::string value = "value-" + std::to_string(key);
            return cli.Put(path.c_str(), value, "text/plain");
        }
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<MixedWorkload>(*this);
    }
};
