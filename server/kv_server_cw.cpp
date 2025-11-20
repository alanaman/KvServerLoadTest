#include <atomic>

#include "kv_server_cw.hpp"
#include "civetweb.h"
#include <regex>
#include <sstream>
#include <cstring>

static int dispatch_handler(struct mg_connection *conn, void *user_data);

KvServerCw::KvServerCw(ConnectionPool<KvDatabase>* dbConnPool,
                   int thread_count,
                   int cache_size)
    : connPool(dbConnPool),
      thread_count(thread_count),
      cache(static_cast<size_t>(cache_size))
{

}

static int dispatch_handler(struct mg_connection *conn, void *user_data)
{
    KvServerCw* self = static_cast<KvServerCw*>(user_data);
    const mg_request_info* info = mg_get_request_info(conn);

    std::string uri(info->local_uri ? info->local_uri : "");
    const char* method = info->request_method ? info->request_method : "";

    // Route "/"
    if (uri == "/" && std::strcmp(method, "GET") == 0) {
        self->HandleRoot(conn);
        return 1;
    }

    // Handle /<number>
    std::regex keyRegex("^/(\\d+)$");
    std::smatch match;
    if (std::regex_match(uri, match, keyRegex)) {
        int key = std::stoi(match[1]);

        if (std::strcmp(method, "GET") == 0) {
            self->GetKv(conn, key);
            return 1;
        }
        else if (std::strcmp(method, "PUT") == 0) {
            self->PutKv(conn, key);
            return 1;
        }
        else if (std::strcmp(method, "DELETE") == 0) {
            self->DeleteKv(conn, key);
            return 1;
        }
    }

    // Unknown route
    mg_printf(conn,
              "HTTP/1.1 404 Not Found\r\n"
              "Content-Type: text/plain\r\n"
              "\r\n"
              "Not Found");
    return 1;
}


void KvServerCw::HandleRoot(struct mg_connection *conn)
{
    std::stringstream ss;
    ss << "totalGets:" << totalGets << "\n"
       << "cacheHits:" << cacheHits << "\n";
    std::string out = ss.str();

    mg_printf(conn,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n%s",
        out.size(), out.c_str());
}


void KvServerCw::GetKv(struct mg_connection *conn, int key)
{
    totalGets++;

    auto val = cache.Get(key);
    if (val.has_value()) {
        cacheHits++;
        send_text(conn, 200, val.value());
        return;
    }

    auto db = connPool->acquire();
    auto v = db->getValueForKey(key);

    if (v.has_value()) {
        cache.Put(key, v.value());
        send_text(conn, 200, v.value());
    } else {
        send_text(conn, 404, "Key not found");
    }
}

void KvServerCw::PutKv(struct mg_connection *conn, int key)
{
    std::string body = read_body(conn);

    auto db = connPool->acquire();
    db->putKeyValue(key, body);

    cache.Remove(key);
    send_text(conn, 200, "Updated");
}


void KvServerCw::DeleteKv(struct mg_connection *conn, int key)
{
    auto db = connPool->acquire();
    db->deleteKeyValue(key);

    cache.Remove(key);
    send_text(conn, 200, "Deleted");
}

std::string KvServerCw::read_body(struct mg_connection *conn)
{
    char buf[8192];
    int len = mg_read(conn, buf, sizeof(buf));
    if (len < 0) len = 0;
    return std::string(buf, (size_t)len);
}


void KvServerCw::send_text(struct mg_connection *conn, int status, const std::string& s)
{
    mg_printf(conn,
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n%s",
        status, s.size(), s.c_str());
}


int KvServerCw::Listen(int port)
{
    const char *options[] = {
        "listening_ports", std::to_string(port).c_str(),
        "num_threads", std::to_string(thread_count).c_str(),
        "tcp_nodelay", "1",
        nullptr
    };

    mg_callbacks callbacks{};
    ctx = mg_start(&callbacks, this, options);

    if (!ctx) {
        throw std::runtime_error("Could not start CivetWeb");
    }

    // Routes
    mg_set_request_handler(ctx, "/", dispatch_handler, this);

    std::cout << "Server running on http://0.0.0.0:" << port << "\n";
    std::cout << "Press Enter to quit...\n";
    getchar();
    return 0;
}
