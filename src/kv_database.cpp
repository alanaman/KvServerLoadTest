#include "kv_database.hpp"
#include <sqlpp11/postgresql/on_conflict.h>
#include <optional>         // For std::optional
#include <sqlpp11/update.h>   // For update()
#include <sqlpp11/remove.h> // For remove_from()
#include <sqlpp11/select.h>   // For select()

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

/**
 * @brief Updates the value for an existing key.
 * If the key does not exist, it prints a warning and does nothing.
 */
void KvDatabase::updateKeyValue(int key, const std::string& value)
{
    try
    {
        auto update_stmt = update(tab).set(
            tab.value = value
        ).where(tab.key == key);

        // db(statement) returns the number of affected rows
        auto result = db(update_stmt);
        
        if (result == 0)
        {
            std::cout << "Warning: Key " << key << " not found. No update performed." << std::endl;
        }
        else
        {
            std::cout << "Successfully updated key " << key << "." << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database error during update: " << e.what() << std::endl;
    }
}

/**
 * @brief Deletes a key-value pair from the database.
 * If the key does not exist, it prints a warning and does nothing.
 */
void KvDatabase::deleteKeyValue(int key)
{
    try
    {
        auto delete_stmt = remove_from(tab).where(tab.key == key);

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
        auto select_stmt = select(tab.value).from(tab).where(tab.key == key);

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