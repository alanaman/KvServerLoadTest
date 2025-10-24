#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/postgresql/connection.h>
#include <sqlpp11/postgresql/exception.h>
#include <sqlpp11/postgresql/on_conflict.h> // <-- Put PostgreSQL headers first

#include <string>
#include <iostream>
// #include <libpq-fe.h> 

// Include our table definition
#include "KeyValue.h"
#include "kv_database.hpp"

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
int main()
{
    KvDatabase kvdb;
    try
    {
        int key;
        std::string value;
        for(int i=0;i<100;i++)
        {
            key = rand()%1000;
            value = random_string(5);
            kvdb.insertKeyValue(key, value);
        }

        
        std::cout << "\nSelecting all keys:" << std::endl;
        
        // auto& row = db(select(all_of(tab)).from(tab).where(tab.id > 1))
        for (const auto& row : kvdb.db(sqlpp::select(all_of(KvDatabase::tab)).from(KvDatabase::tab).unconditionally()))
        {
            std::cout << "  Key: " << row.key << ", Value: " << row.value << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}