#include <string>
#include <iostream>

#include "KeyValue.h"
#include "kv_database.hpp"
#include "kv_server.hpp"
// #include "kv_server_cw.hpp"

#include <httplib.h>

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <threads>\n";
        return 1;
    }

    int num_threads = std::stoi(argv[1]);
    KvDatabase kvdb;
    kvdb.Bootstrap();

    auto factory = []() {
        try {
            auto conn = std::make_unique<KvDatabase>();
            std::cout << "[Factory] New connection created." << std::endl;
            return conn;
        } catch (const std::exception& e) {
            std::cerr << "[Factory] Failed to create connection: " << e.what() << std::endl;
            return std::unique_ptr<KvDatabase>(nullptr);
        }
    };

    KvServer svr(new ConnectionPool<KvDatabase>(num_threads, factory), num_threads);

    svr.Listen();

    return 0;
}