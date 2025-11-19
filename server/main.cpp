#include <string>
#include <iostream>

#include "KeyValue.h"
#include "kv_database.hpp"
#include "kv_server.hpp"

#include <httplib.h>

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <port> <dbhost> <threads>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string db_host=argv[2];    
    int num_threads = std::stoi(argv[3]);

    KvDatabase kvdb(db_host);
    kvdb.Bootstrap();
    kvdb.PrepareStatements();

    auto factory = [db_host]() {
        try {
            auto conn = std::make_unique<KvDatabase>(db_host);
            std::cout << "[Factory] New connection created." << std::endl;
            return conn;
        } catch (const std::exception& e) {
            std::cerr << "[Factory] Failed to create connection: " << e.what() << std::endl;
            return std::unique_ptr<KvDatabase>(nullptr);
        }
    };

    KvServer svr(new ConnectionPool<KvDatabase>(num_threads, factory), num_threads);

    svr.Listen(port);

    return 0;
}