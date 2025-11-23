#include "kv_server.hpp"

std::atomic<long long> KvServer::active_thread_count{0};

KvServer::KvServer(ConnectionPool<KvDatabase>* dbConnPool, int thread_count, int cache_size):
    connPool(dbConnPool), cache(static_cast<size_t>(cache_size))
{
    server.new_task_queue = [thread_count]{
        return new httplib::ThreadPool(thread_count, thread_count);
    };

    server.set_tcp_nodelay(true);
    
    server.Get("/", [this](const httplib::Request & /*req*/, httplib::Response &res) {
        std::stringstream ss;
        ss<<"totalGets:"<<totalGets<<std::endl;
        ss<<"cacheHits:"<<cacheHits<<std::endl;
        res.set_content(ss.str(), "text/plain");
    });

    server.Get("/(\\d+)", [this](const httplib::Request &req, httplib::Response &res) {
        GetKv(req, res);
    });
    server.Put("/(\\d+)", [this](const httplib::Request &req, httplib::Response &res) {
        PutKv(req, res);
    });
    server.Delete("/(\\d+)", [this](const httplib::Request &req, httplib::Response &res) {
        DeleteKv(req, res);
    });
}

void KvServer::GetKv(const httplib::Request &req, httplib::Response &res)
{
    // res.status = 200; // OK
    // return;
    int key = std::stoi(req.matches[1].str());

    totalGets++;
    
    auto val = cache.Get(key);
    if(val.has_value())
    {
        res.set_content(val.value(), "text/plain");
        res.status = 200; // OK
        cacheHits++;
        return;
    }
    
    auto database = connPool->acquire();
    auto opt_value = database->getValueForKey(key);

    if (opt_value.has_value())
    {
        res.set_content(opt_value.value(), "text/plain");
        res.status = 200; // OK

        cache.Put(key, opt_value.value());
    }
    else
    {
        res.set_content("Key not found", "text/plain");
        res.status = 404; // Not Found
    }
}

void KvServer::PutKv(const httplib::Request &req, httplib::Response &res)
{
    // try
    // {
        int key = std::stoi(req.matches[1].str());
        std::string value = req.body;
        auto database = connPool->acquire();
        

        database->putKeyValue(key, value);
        cache.Remove(key); // Invalidate cache entry if it exists
        // res.set_content("Updated", "text/plain");
        res.status = 200; // OK
    // }
    // catch (const std::invalid_argument &e)
    // {
    //     res.set_content("Invalid key format. Key must be an integer.", "text/plain");
    //     res.status = 400; // Bad Request
    // }
    // catch (const std::exception &e)
    // {
    //     // Handle potential database errors
    //     res.set_content("Database error: " + std::string(e.what()), "text/plain");
    //     res.status = 500; // Internal Server Error
    // }
}

void KvServer::DeleteKv(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto database = connPool->acquire();

        int key = std::stoi(req.matches[1].str());
        database->deleteKeyValue(key);
        cache.Remove(key); // Invalidate cache entry if it exists
        res.set_content("Deleted", "text/plain");
        res.status = 200; // OK
    }
    catch (const std::invalid_argument &e)
    {
        res.set_content("Invalid key format. Key must be an integer.", "text/plain");
        res.status = 400; // Bad Request
    }
    catch (const std::exception &e)
    {
        // Handle potential database errors
        res.set_content("Database error: " + std::string(e.what()), "text/plain");
        res.status = 500; // Internal Server Error
    }
}

int KvServer::Listen(int port)
{
    std::cout << "Starting server on http://0.0.0.0:" << port << std::endl;
    if (!server.listen("0.0.0.0", port))
    {
        std::cerr << "Failed to start server!" << std::endl;
        return -1;
    }
    return 0;
}
