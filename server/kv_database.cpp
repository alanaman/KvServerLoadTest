#include "kv_database.hpp"
#include <sqlpp11/postgresql/on_conflict.h>
#include <optional>         // For std::optional
#include <sqlpp11/update.h>   // For update()
#include <sqlpp11/remove.h> // For remove_from()
#include <sqlpp11/select.h>   // For select()

#include <sqlpp11/parameter.h>                      // For sqlpp::parameter
#include <sqlpp11/alias_provider.h>                 // For SQLPP_ALIASROVIDER
// #include <sqlpp11/data_types/integer/data_type.h>   // For sqlpp::integer
#include <sqlpp11/data_types/text/data_type.h>      // For sqlpp::text

// // --- ADD THESE ---
// // Define the *types* for our parameter names
// SQLPP_ALIASROVIDER(key)
// SQLPP_ALIASROVIDER(value)

tables::KeyValueTable KvDatabase::kv_table;

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
}

void KvDatabase::Bootstrap()
{
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

void KvDatabase::PrepareStatements()
{
    prepared_insert = db.prepare(_get_prepared_insert_type());
    prepared_update = db.prepare(_get_prepared_update_type());
    prepared_select = db.prepare(_get_prepared_select_type());
}

void KvDatabase::insertKeyValue(int key, const std::string &value)
{
    auto insert = sqlpp::postgresql::insert_into(kv_table).set(
        kv_table.key = key,
        kv_table.value = value
    );
    db(insert.on_conflict().do_nothing());
}

void KvDatabase::insertKeyValueSafe(int key, const std::string &value)
{
    try
    {
        // Prepare the insert statement
        auto insert = insert_into(kv_table).set(
            kv_table.key = key,
            kv_table.value = value
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

/**
 * @brief Updates the value for an existing key.
 * If the key does not exist, it prints a warning and does nothing.
 */
void KvDatabase::updateKeyValue(int key, const std::string& value)
{
    try
    {
        auto update_stmt = update(kv_table).set(
            kv_table.value = value
        ).where(kv_table.key == key);

        // db(statement) returns the number of affected rows
        auto result = db(update_stmt);
        
        if (result == 0)
        {
            std::cout << "Warning: Key " << key << " not found. No update performed." << std::endl;
        }
        else
        {
            // std::cout << "Successfully updated key " << key << "." << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during update: " << e.what() << std::endl;
    }
}

void KvDatabase::insertKeyValuePrep(int key, const std::string &value)
{
    prepared_insert.params.key = key;
    prepared_insert.params.value = value;

    db(prepared_insert);
}

/**
 * @brief Deletes a key-value pair from the database.
 * If the key does not exist, it prints a warning and does nothing.
 */
void KvDatabase::deleteKeyValue(int key)
{
    try
    {
        auto delete_stmt = remove_from(kv_table).where(kv_table.key == key);

        // db(statement) returns the number of affected rows
        auto result = db(delete_stmt);

        if (result == 0)
        {
            std::cout << "Warning: Key " << key << " not found. No deletion performed." << std::endl;
        }
        else
        {
            std::cout << "Successfully deleted key " << key << "." << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during delete: " << e.what() << std::endl;
    }
}

/**
 * @brief Retrieves the value for a given key.
 * @return std::optional<std::string> containing the value if found, 
 * or std::nullopt if the key does not exist or an error occurs.
 */
std::optional<std::string> KvDatabase::getValueForKey(int key)
{
    try
    {
        auto select_stmt = select(kv_table.value).from(kv_table).where(kv_table.key == key);

        // std::ostringstream os;
        // sqlpp::serialize(select_stmt, os);
        // std::cout << os.str() << std::endl;

        // Use a loop to process the result set.
        // Since 'key' is a PRIMARY KEY, this loop will run at most once.
        for (const auto& row : db(select_stmt))
        {
            // Key was found, return the value
            return row.value;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during select: " << e.what() << std::endl;
    }

    // Key not found or an error occurred
    return std::nullopt;
}

void KvDatabase::putKeyValue(int key, const std::string &value)
{
    db(sqlpp::postgresql::insert_into(kv_table).set(
        kv_table.key = key,
        kv_table.value = value
      )
      .on_conflict(kv_table.key)
      .do_update(
        kv_table.value = value
      )
    );
}

double KvDatabase::testInsertThroughput(size_t num_operations)
{
    std::cout << "Testing INSERT throughput with " << num_operations << " operations..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    try
    {
        db.start_transaction(); // Start one big transaction

        for (size_t i = 0; i < num_operations; ++i)
        {
            prepared_insert.params.key = static_cast<int>(i);
            prepared_insert.params.value = "value_" + std::to_string(i);
            db(prepared_insert);
            // auto insert = sqlpp::postgresql::insert_into(tab).set(
            //     tab.key = static_cast<int>(rand()%10000),
            //     tab.value = "value_" + std::to_string(i)
            // );
            // db(insert.on_conflict().do_nothing());
        }
        
        db.commit_transaction(); // Commit all operations at once
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during insert benchmark: " << e.what() << std::endl;
        return 0.0; // Return 0 on failure
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_sec = end - start;

    double throughput = static_cast<double>(num_operations) / duration_sec.count();
    std::cout << "Insert Test Complete: " << throughput << " ops/sec" << std::endl;
    return throughput;
}


double KvDatabase::testUpdateThroughput(size_t num_operations)
{
    std::cout << "Testing UPDATE throughput with " << num_operations << " operations..." << std::endl;
    
    // Note: This test assumes keys 0 to (num_operations-1) exist!
    // You should run testInsertThroughput(N) before testUpdateThroughput(N).

    auto start = std::chrono::high_resolution_clock::now();

    try
    {
        db.start_transaction();

        for (size_t i = 0; i < num_operations; ++i)
        {
            prepared_update.params.key = static_cast<int>(i);
            prepared_update.params.value = "new_value_" + std::to_string(i);
            db(prepared_update);
        }
        
        db.commit_transaction();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during update benchmark: " << e.what() << std::endl;
        return 0.0;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_sec = end - start;

    double throughput = static_cast<double>(num_operations) / duration_sec.count();
    std::cout << "Update Test Complete: " << throughput << " ops/sec" << std::endl;
    return throughput;
}


double KvDatabase::testReadThroughput(size_t num_operations)
{
    std::cout << "Testing READ throughput with " << num_operations << " operations..." << std::endl;

    // Note: This test assumes keys 0 to (num_operations-1) exist!
    
    std::string value_buffer; // To "use" the data
    auto start = std::chrono::high_resolution_clock::now();

    try
    {
        db.start_transaction(); // Reads also benefit from a transaction context

        for (size_t i = 0; i < num_operations; ++i)
        {
            prepared_select.params.key = static_cast<int>(i);
            
            // We must consume the result for a fair test
            for (const auto& row : db(prepared_select))
            {
                value_buffer = row.value; 
            }
        }
        
        db.commit_transaction(); // Technically read-only, but good practice
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during read benchmark: " << e.what() << std::endl;
        return 0.0;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_sec = end - start;

    double throughput = static_cast<double>(num_operations) / duration_sec.count();
    std::cout << "Read Test Complete: " << throughput << " ops/sec" << std::endl;
    return throughput;
}