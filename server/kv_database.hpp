#pragma once

#include <string>
#include <optional>
#include <any>

#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/select.h>
#include <sqlpp11/postgresql/prepared_statement.h>
#include <sqlpp11/postgresql/on_conflict.h>
#include <sqlpp11/postgresql/insert.h>
#include <sqlpp11/verbatim.h>
#include "KeyValue.h"

namespace {
    // Helper to get the INSERT statement type
    auto _get_prepared_insert_type() {
        tables::KeyValueTable tab;
        auto insert = sqlpp::postgresql::insert_into(tab).set(
            tab.key = sqlpp::parameter(tab.key),
            tab.value = sqlpp::parameter(tab.value)
        );

        // Use excluded(tab.value) instead of parameter(tab.value)
        return insert.on_conflict(tab.key).do_update(
            tab.value = sqlpp::verbatim<sqlpp::text>("EXCLUDED.value")
        );
        // return insert.on_conflict(tab.key).do_nothing();
        // return insert;
    }
}


class KvDatabase
{
public:
    static tables::KeyValueTable kv_table;
    
    sqlpp::postgresql::connection db;
    
    using PreparedInsert_t = decltype(db.prepare(_get_prepared_insert_type()));
    PreparedInsert_t prepared_insert;

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