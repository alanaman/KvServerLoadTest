#pragma once

#include "civetweb.h"
#include "kv_database.hpp"
#include "db_conn_pool.hpp"
#include "caches/CoarseLockCache.hpp"
#include "caches/ShardedCache.hpp"

class KvServerCw
{
public:
    static std::atomic<long long> thread_count;

    KvServerCw(ConnectionPool<KvDatabase>* dbConnPool,
             int thread_count=10,
             int cache_size=1024);

    int Listen();

    // CivetWeb context
    mg_context* ctx = nullptr;

    // external components
    ConnectionPool<KvDatabase>* connPool;
    FineLRUCache<int, std::string> cache;
    
    long long totalGets = 0;
    long long cacheHits = 0;
    
    // Handlers
    void HandleRoot(struct mg_connection *conn);
    void GetKv(struct mg_connection *conn, int key);
    void PutKv(struct mg_connection *conn, int key);
    void DeleteKv(struct mg_connection *conn, int key);
    
    // Helper utilities
    std::string read_body(struct mg_connection *conn);
    void send_text(struct mg_connection *conn, int status, const std::string &s);
};