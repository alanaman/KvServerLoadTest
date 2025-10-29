#include <string>
#include <iostream>

#include "KeyValue.h"
#include "kv_database.hpp"
#include "kv_server.hpp"

#include <httplib.h>

std::string random_string(int length)
{
    // Define the set of characters to choose from
    static const std::string charset =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    // Set up the random number generator
    // We use 'static' so the generator is seeded only once
    static std::mt19937 gen(std::random_device{}());
    
    // Define the distribution (uniform over all character indices)
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);

    // Build the result string
    std::string result;
    result.reserve(length); // Pre-allocate memory for efficiency

    for (int i = 0; i < length; ++i)
    {
        result += charset[dis(gen)];
    }

    return result;
}
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

    try
    {
        int key;
        std::string value;
        for(int i=0;i<10;i++)
        {
            key = rand()%1000;
            value = random_string(5);
            kvdb.insertKeyValuePrep(key, value);
        }
        
        kvdb.insertKeyValue(1, "hello");
        kvdb.insertKeyValue(2, "hello");
        kvdb.insertKeyValue(3, "hello");
        kvdb.deleteKeyValue(1);
        kvdb.updateKeyValue(2, "else");
        std::cout << kvdb.getValueForKey(3).value() << std::endl;


        
        std::cout << "\nSelecting all keys:" << std::endl;
        
        // auto& row = db(select(all_of(tab)).from(tab).where(tab.id > 1))
        for (const auto& row : kvdb.db(sqlpp::select(all_of(KvDatabase::tab)).from(KvDatabase::tab).unconditionally()))
        {
            std::cout << "  Key: " << row.key << ", Value: " << row.value << std::endl;
        }

        kvdb.testInsertThroughput(100000);
        kvdb.testUpdateThroughput(100000);
        kvdb.testReadThroughput(100000);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}