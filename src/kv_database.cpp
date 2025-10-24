#include "kv_database.hpp"
#include <sqlpp11/postgresql/on_conflict.h>

tables::KeyValueTable KvDatabase::tab;

KvDatabase::KvDatabase()
{
    auto config = std::make_shared<sqlpp::postgresql::connection_config>();
    
    config->dbname = "kv_db";
    config->user = "kv_app";
    config->password = "mysecretpassword";
    config->host = "postgres-db";
    config->port = 5432;
    // config->debug = true; // Uncomment for verbose debugging output

    try
    {
        db.connectUsing(config);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Could not connect to database: " << e.what() << std::endl;
    }

    try
    {
        db.execute("DROP TABLE IF EXISTS key_value;");
        db.execute("CREATE TABLE key_value (key INTEGER PRIMARY KEY, value TEXT NOT NULL);");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
    }        
}

void KvDatabase::insertKeyValue(int key, const std::string &value)
{
    auto insert = sqlpp::postgresql::insert_into(tab).set(
        tab.key = key,
        tab.value = value
    );
    db(insert.on_conflict().do_nothing());
}

void KvDatabase::insertKeyValueSafe(int key, const std::string &value)
{
    try
    {
        // Prepare the insert statement
        auto insert = insert_into(tab).set(
            tab.key = key,
            tab.value = value
        );

        // Execute the insert
        db(insert);
        std::cout << "Successfully inserted key " << key << "." << std::endl;
    }
    catch (const sqlpp::postgresql::data_exception& ex)
    {
        // This block handles database-level errors gracefully.
        std::string error_msg = ex.what();

        // PostgreSQL error code for "unique_violation" is "23505"
        // This is how we "abort if key is already present"
        if (error_msg.find("23505") != std::string::npos)
        {
            std::cerr << "Error: Key " << key << " already exists. Aborting insert." << std::endl;
        }
        else
        {
            // Handle other potential database errors
            std::cerr << "Database error: " << error_msg << std::endl;
        }
    }
    catch (const std::exception& ex)
    {
        // Handle other non-database C++ exceptions
        std::cerr << "Generic error: " << ex.what() << std::endl;
    }
}
