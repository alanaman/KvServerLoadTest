/*
 * ======================================================================
 * Local KvServer::GetKv Throughput Tester
 *
 * How to Compile:
 * 1. Add this file to your project's source directory.
 * 2. Compile it along with your other project files (KvServer.cpp,
 * KvDatabase.cpp, ConnectionPool.cpp, etc.).
 * 3. Link everything together, making sure to link against 'pthread'.
 *
 * Example (using g++):
 * g++ -std=c++17 -O2 -o local_tester \
 * LocalThroughputTester.cpp \
 * KvServer.cpp \
 * KvDatabase.cpp \
 * # ... other .cpp files or .o files ...
 * -pthread -lsqlite3 # ... other libs ...
 *
 * How to Run:
 * ./local_tester <num_threads> <duration_sec> <cache_size> <max_key>
 *
 * Example:
 * ./local_tester 8 10 1000 10000
 * (Tests with 8 threads for 10s, 1000-item cache, keys 0-9999)
 * ======================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <optional>
#include <functional>
#include <sstream>
#include <iomanip> // For std::setprecision

// --- Include your project's headers ---
// Adjust these paths if necessary
#include "kv_server.hpp"
#include "kv_database.hpp"
#include "db_conn_pool.hpp"
#include "httplib.h" // We need the real httplib::Request/Response types

// --- Globals for testing ---
std::atomic<bool> keep_running(true);

struct ThreadStats {
    long long requests = 0;
    long long errors = 0;
    long long duration_micros = 0;
};

// --- Local Workload Definition ---
// This interface and class are specific to this tester
class ILocalWorkload {
public:
    virtual ~ILocalWorkload() = default;
    // Generates the next key to request
    virtual int next_key(std::mt19937& gen) = 0;
};

class UniformGetWorkload : public ILocalWorkload {
public:
    UniformGetWorkload(int max_key)
        : dist(0, max_key - 1) // Keys 0 to max_key-1
    {
        if (max_key <= 0) {
            std::cerr << "Max key must be > 0. Setting to 1." << std::endl;
            dist = std::uniform_int_distribution<int>(0, 0);
        }
    }

    int next_key(std::mt19937& gen) override {
        return dist(gen);
    }
private:
    std::uniform_int_distribution<int> dist;
};


// --- Local Worker Thread ---
// This is the local equivalent of your `client_worker`
void local_worker(KvServer* server,
                  std::unique_ptr<ILocalWorkload> workload,
                  int seed,
                  ThreadStats* stats) // Pass stats out via pointer
{
    // Seed the random number generator
    std::mt19937 gen;
    if (seed == -1) {
        gen.seed(std::random_device{}());
    } else {
        gen.seed(seed);
    }

    long long thread_requests = 0;
    long long thread_errors = 0;
    long long thread_duration_micros = 0;

    // Pre-allocate request/response objects to avoid allocation in the loop
    // We use the *real* httplib types from your project
    httplib::Request req;
    req.matches.resize(2); // We need req.matches[1] for GetKv
    httplib::Response res;

    while (keep_running.load(std::memory_order_relaxed)) {
        // 1. Generate workload
        int key = workload->next_key(gen);
        std::string key_str = std::to_string(key);

        // 2. Prepare mock request
        // We are "faking" a request from the network
        // req.path = "/key/" + key_str; // For logging, if needed
        req.matches[1] = key_str; // This is what GetKv uses

        // 3. Reset response
        res.status = -1;
        // res.body.clear(); // set_content overwrites, no need to clear

        // 4. Time the call
        auto start_time = std::chrono::steady_clock::now();

        server->GetKv(req, res); // <<< THE DIRECT, LOCAL CALL

        auto end_time = std::chrono::steady_clock::now();

        // 5. Validate and record
        // We count 200 (Hit) and 404 (Miss) as successful operations
        if (res.status == 200 || res.status == 404) {
            thread_requests++;
            thread_duration_micros += std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time
            ).count();
        } else {
            // This would catch -1 or other unexpected statuses
            thread_errors++;
        }
    }

    // 7. Store results
    stats->requests = thread_requests;
    stats->errors = thread_errors;
    stats->duration_micros = thread_duration_micros;
}


// --- MAIN DRIVER ---
int main(int argc, char* argv[]) {
    // --- Parse Arguments ---
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <num_threads> <duration_sec> <cache_size> <max_key>" << std::endl;
        return 1;
    }

    int num_threads = std::stoi(argv[1]);
    int duration_sec = std::stoi(argv[2]);
    int cache_size = std::stoi(argv[3]);
    int max_key = std::stoi(argv[4]); // For workload, e.g., 10000

    if (num_threads <= 0 || duration_sec <= 0 || cache_size <= 0 || max_key <= 0) {
        std::cerr << "All arguments must be positive integers." << std::endl;
        return 1;
    }

    std::cout << "Starting local GetKv throughput test with:" << std::endl;
    std::cout << "  Threads:      " << num_threads << std::endl;
    std::cout << "  Duration:     " << duration_sec << " seconds" << std::endl;
    std::cout << "  Cache Size:   " << cache_size << " items" << std::endl;
    std::cout << "  Key Range:    " << "0 - " << (max_key - 1) << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    // --- Setup Server ---
    // 1. Create the DB factory (as seen in your server's main)
    auto factory = []() {
        try {
            auto conn = std::make_unique<KvDatabase>();
            // We can silence this for the test
            // std::cout << "[Factory] New connection created." << std::endl;
            return conn;
        } catch (const std::exception& e) {
            std::cerr << "[Factory] Failed to create connection: " << e.what() << std::endl;
            return std::unique_ptr<KvDatabase>(nullptr);
        }
    };
    
    // 2. Create the real ConnectionPool
    auto* pool = new ConnectionPool<KvDatabase>(num_threads, factory);

    // 3. Create the real KvServer instance
    // *** NOTE: Using constructor from your FIRST prompt ***
    // KvServer(ConnectionPool<KvDatabase>* dbConnPool, int thread_count, int cache_size)
    KvServer server(pool, num_threads, cache_size);
    // If your constructor is different (e.g., no cache_size),
    // you must change the line above to match your project.

    // 4. *** DO NOT CALL server.Listen() ***
    // We are testing GetKv directly.

    // --- Setup Threads ---
    std::vector<std::thread> threads;
    std::vector<ThreadStats> all_stats(num_threads);

    // --- Start Test ---
    auto test_start_time = std::chrono::steady_clock::now();
    for (int i = 0; i < num_threads; ++i) {
        // Each thread gets its own workload generator
        std::unique_ptr<ILocalWorkload> workload = std::make_unique<UniformGetWorkload>(max_key);
        // Give each thread a unique, deterministic seed
        int seed = i;
        threads.emplace_back(local_worker, &server, std::move(workload), seed, &all_stats[i]);
    }

    // --- Run for Duration ---
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

    // --- Stop Test ---
    keep_running.store(false, std::memory_order_relaxed);

    long long total_requests = 0;
    long long total_errors = 0;
    long long total_duration_micros = 0;

    for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
        total_requests += all_stats[i].requests;
        total_errors += all_stats[i].errors;
        total_duration_micros += all_stats[i].duration_micros;
    }
    auto test_end_time = std::chrono::steady_clock::now();

    // --- Report Results ---
    double total_duration_sec = std::chrono::duration<double>(test_end_time - test_start_time).count();

    double throughput = static_cast<double>(total_requests) / total_duration_sec;
    double avg_latency_us = (total_requests == 0) ? 0 :
        static_cast<double>(total_duration_micros) / static_cast<double>(total_requests);

    // Read stats directly from the server instance
    long long final_gets = server.totalGets.load();
    long long final_hits = server.cacheHits.load();
    double cache_hit_rate = (final_gets == 0) ? 0 :
        static_cast<double>(final_hits) / static_cast<double>(final_gets) * 100.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Test Finished." << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total Requests:   " << total_requests << std::endl;
    std::cout << "Total Errors:     " << total_errors << std::endl;
    std::cout << "Total Test Time:  " << total_duration_sec << " s" << std::endl;
    std::cout << "------------------" << std::endl;
    std::cout << "Throughput:       " << throughput << " req/s" << std::endl;
    std::cout << "Avg. Latency:     " << avg_latency_us << " us" << std::endl;
    std::cout << "------------------" << std::endl;
    std::cout << "Total Gets (raw): " << final_gets << std::endl;
    std::cout << "Cache Hits (raw): " << final_hits << std::endl;
    std::cout << "Cache Hit Rate:   " << cache_hit_rate << " %" << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    // Clean up the connection pool
    delete pool;

    return 0;
}