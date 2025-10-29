#pragma once

#include <httplib.h>
#include "kv_database.hpp"
#include "db_conn_pool.hpp"

class KvServer
{
    ConnectionPool<KvDatabase>* connPool;
    httplib::Server server;
public:

    KvServer(ConnectionPool<KvDatabase>* dbConnPool, int thread_count=10);

    void GetKv(const httplib::Request &req, httplib::Response &res);
    
    void PutKv(const httplib::Request &req, httplib::Response &res);

    void DeleteKv(const httplib::Request &req, httplib::Response &res);

    int Listen();
};