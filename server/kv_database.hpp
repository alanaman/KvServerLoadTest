#pragma once

#include <string>
#include <optional>
#include <any>

#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/select.h>
#include <sqlpp11/postgresql/prepared_statement.h>
#include "KeyValue.h"

namespace {
    // Helper to get the INSERT statement type
    auto _get_prepared_insert_type() {
        tables::KeyValueTable tab;
        auto insert = sqlpp::postgresql::insert_into(tab).set(
            tab.key = sqlpp::parameter(tab.key),
            tab.value = sqlpp::parameter(tab.value)
        );
        return insert.on_conflict().do_nothing();
    }

    // Helper to get the UPDATE statement type
    auto _get_prepared_update_type() {
        tables::KeyValueTable tab;
        return update(tab).set(
            tab.value = sqlpp::parameter(tab.value)
        ).where(tab.key == sqlpp::parameter(tab.key));
    }

    // // Helper to get the SELECT statement type
    auto _get_prepared_select_type() {
        tables::KeyValueTable tab;
        return sqlpp::select(tab.value).from(tab).where(tab.key == sqlpp::parameter(tab.key));
    }
}

class KvDatabase
{
public:
    static tables::KeyValueTable kv_table;
    
    
    sqlpp::postgresql::connection db;
    using PreparedInsert_t = decltype(db.prepare(_get_prepared_insert_type()));
    using PreparedUpdate_t = decltype(db.prepare(_get_prepared_update_type()));
    using PreparedSelect_t = decltype(db.prepare(_get_prepared_select_type()));

    PreparedInsert_t prepared_insert;
    PreparedUpdate_t prepared_update;
    PreparedSelect_t prepared_select;

    KvDatabase();

    void Bootstrap();
    

    void insertKeyValueSafe(int key, const std::string& value);
    void insertKeyValue(int key, const std::string& value);
    void insertKeyValuePrep(int key, const std::string& value);
    void updateKeyValue(int key, const std::string& value);

    std::optional<std::string> getValueForKey(int key);
    void putKeyValue(int key,  const std::string& value);
    void deleteKeyValue(int key);

    void PrepareStatements();

    double testInsertThroughput(size_t num_operations);
    double testUpdateThroughput(size_t num_operations);
    double testReadThroughput(size_t num_operations);
};