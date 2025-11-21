#pragma once

#include <string>
#include <optional>
#include <any>

#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/select.h>
#include <sqlpp11/postgresql/prepared_statement.h>
#include "KeyValue.h"


class KvDatabase
{
public:
    static tables::KeyValueTable kv_table;
    
    
    sqlpp::postgresql::connection db;

    KvDatabase(std::string db_hostname = "postgres-db");

    void Bootstrap();
    

    void insertKeyValueSafe(int key, const std::string& value);
    void insertKeyValue(int key, const std::string& value);
    void insertKeyValuePrep(int key, const std::string& value);
    void updateKeyValue(int key, const std::string& value);

    std::optional<std::string> getValueForKey(int key);
    void putKeyValue(int key,  const std::string& value);
    void deleteKeyValue(int key);

    double testInsertThroughput(size_t num_operations);
    double testUpdateThroughput(size_t num_operations);
    double testReadThroughput(size_t num_operations);
};