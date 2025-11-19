#pragma once

#include <httplib.h>
#include "kv_database.hpp"
#include "db_conn_pool.hpp"
#include "caches/CoarseLockCache.hpp"
#include "caches/ShardedCache.hpp"

class KvServer
{
    ConnectionPool<KvDatabase>* connPool;
    httplib::Server server;
    FineLRUCache<int, std::string> cache;

    static std::atomic<long long> active_thread_count;

public:
    int totalGets=0;
    int cacheHits=0;

    KvServer(ConnectionPool<KvDatabase>* dbConnPool, int thread_count=10, int cache_size=1024);

    void GetKv(const httplib::Request &req, httplib::Response &res);
    
    void PutKv(const httplib::Request &req, httplib::Response &res);

    void DeleteKv(const httplib::Request &req, httplib::Response &res);

    int Listen(int port);
};