#pragma once

#include <httplib.h>
#include "kv_database.hpp"

class KvServer
{
    KvDatabase* database;
    httplib::Server server;
public:

    KvServer(KvDatabase* database, int thread_count=10);

    void GetKv(const httplib::Request &req, httplib::Response &res);
    
    void PutKv(const httplib::Request &req, httplib::Response &res);

    void DeleteKv(const httplib::Request &req, httplib::Response &res);

    int Listen();
};