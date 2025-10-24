#include <string>
#include <optional>
#include <sqlpp11/postgresql/postgresql.h>
#include "KeyValue.h"


class KvDatabase
{
public:
    static tables::KeyValueTable tab;


    sqlpp::postgresql::connection db;
    KvDatabase();
    

    void insertKeyValueSafe(int key, const std::string& value);
    void insertKeyValue(int key, const std::string& value);
    void updateKeyValue(int key, const std::string& value);
    void deleteKeyValue(int key);
    std::optional<std::string> getValueForKey(int key);
};