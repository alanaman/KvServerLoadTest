#pragma once

#include "httplib.h"
#include "workload.hpp"


/**
 * @brief Workload c: "get_popular"
 * Performs GET requests from a small, "popular" set of keys (1-100).
 * Pre-populates these keys in the prepare() step.
 */
class GetPopularWorkload : public IWorkload {
    std::uniform_int_distribution<> popular_dist;
public:
    GetPopularWorkload() : popular_dist(1, 100) {}

    void prepare(httplib::Client& cli) override {
        // std::cout << "   Preparing popular keys (1-100)...\n";
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
        int key = popular_dist(gen);
        std::string path = "/" + std::to_string(key);
        return cli.Get(path.c_str());
        httplib::Result res;
        return res;
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<GetPopularWorkload>(*this);
    }
};