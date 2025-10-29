#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <stdexcept>
#include <functional> // Required for std::function

// Single-header client/server library
// Download from: https://github.com/yhirose/cpp-httplib
#include "httplib.h"

// --- Global Atomic Counters & Stop Flag ---

std::atomic<bool> keep_running{false};
std::atomic<long long> total_requests{0};
std::atomic<long long> total_errors{0};
std::atomic<int> global_key_counter{0};


/**
 * @brief A function type (using std::function) that defines a single
 * workload operation.
 *
 * It takes a reference to the HTTP client and a random number generator,
 * and it returns the result of the HTTP operation.
 */
using WorkloadFn = std::function<httplib::Result(
    httplib::Client&,
    std::mt19937&
)>;

/**
 * @brief The refactored main function for each client thread.
 *
 * This function now takes a 'workload_op' function object. It no longer
 * performs any string comparisons or branching inside its loop.
 *
 * @param host        Server hostname (e.g., "localhost")
 * @param port        Server port (e.g., 8080)
 * @param workload_op The specific function to call for each request (e.g., a 
 * lambda for "put_all" or "get_popular").
 */
void client_worker(const std::string host, int port, WorkloadFn workload_op) {
    // Each thread gets its own persistent client and random number generator
    httplib::Client cli(host, port);
    cli.set_connection_timeout(5); // 5-second timeout
    std::mt19937 gen(std::random_device{}());

    long long thread_requests = 0;
    long long thread_errors = 0;

    // Closed-loop: run until main thread signals to stop
    while (keep_running.load()) {
        try {
            // --- Workload Generation Logic ---
            // No more if/else! Just call the function passed to us.
            httplib::Result res = workload_op(cli, gen);

            // --- Response Validation ---
            if (res && res->status == 200) {
                thread_requests++;
            } else {
                thread_errors++;
            }
        } catch (const std::exception& e) {
            // Catch any exceptions during the request (e.g., connection issues)
            thread_errors++;
        }
    }

    // Add this thread's counts to the global totals
    total_requests.fetch_add(thread_requests);
    total_errors.fetch_add(thread_errors);
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <host> <port> <threads> <duration_sec> <workload_type>\n"
                  << "Workload Types: put_all, get_all, get_popular, mixed\n"
                  << "Example: " << argv[0] << " localhost 8080 16 30 get_popular\n";
        return 1;
    }

    std::string host;
    int port;
    int num_threads;
    int duration_sec;
    std::string workload_type;
    WorkloadFn workload_op; // The function object we will build

    try {
        host = argv[1];
        port = std::stoi(argv[2]);
        num_threads = std::stoi(argv[3]);
        duration_sec = std::stoi(argv[4]);
        workload_type = argv[5];

        // --- Workload Function Definition ---
        // We do the check ONCE here in main, creating a specialized
        // lambda function for the chosen workload.

        if (workload_type == "put_all") {
            // a. Put all: Unique create requests
            workload_op = [](httplib::Client& cli, std::mt19937& /*gen*/) {
                int key = global_key_counter.fetch_add(1);
                std::string path = "/key/" + std::to_string(key);
                std::string value = "value-" + std::to_string(key);
                return cli.Put(path.c_str(), value, "text/plain");
            };
        } else if (workload_type == "get_all") {
            // b. Get all: Unique read requests (guaranteed cache miss)
            workload_op = [](httplib::Client& cli, std::mt19937& /*gen*/) {
                int key = global_key_counter.fetch_add(1);
                std::string path = "/key/" + std::to_string(key);
                return cli.Get(path.c_str());
            };
        } else if (workload_type == "get_popular") {
            // c. Get popular: Reads from a small set of keys
            // We create the distribution *once* and capture it by value
            std::uniform_int_distribution<> popular_dist(1, 100);
            workload_op = [popular_dist](httplib::Client& cli, std::mt19937& gen) mutable {
                int key = popular_dist(gen); // Key between 1 and 100
                std::string path = "/key/" + std::to_string(key);
                return cli.Get(path.c_str());
            };
        } else if (workload_type == "mixed") {
            // d. Get+Put: 80% popular reads, 20% unique writes
            std::uniform_int_distribution<> popular_dist(1, 100);
            std::uniform_int_distribution<> mix_dist(0, 99);
            workload_op = [popular_dist, mix_dist](httplib::Client& cli, std::mt19937& gen) mutable {
                int op = mix_dist(gen); // 0-99
                
                if (op < 80) { // 80% chance: Get popular
                    int key = popular_dist(gen);
                    std::string path = "/key/" + std::to_string(key);
                    return cli.Get(path.c_str());
                } else { // 20% chance: Put unique
                    int key = global_key_counter.fetch_add(1);
                    std::string path = "/key/" + std::to_string(key);
                    std::string value = "value-" + std::to_string(key);
                    return cli.Put(path.c_str(), value, "text/plain");
                }
            };
        } else {
            throw std::invalid_argument("Invalid workload type.");
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << "\n";
        return 1;
    }

    std::cout << "🚀 Starting load test...\n"
              << "   Target:    http://" << host << ":" << port << "\n"
              << "   Clients:   " << num_threads << "\n"
              << "   Duration:  " << duration_sec << " seconds\n"
              << "   Workload:  " << workload_type << " (Optimized)\n\n";

    std::vector<std::thread> threads;
    keep_running.store(true);

    // --- Spawn Client Threads ---
    for (int i = 0; i < num_threads; ++i) {
        // Pass the *specific workload function* to the thread
        threads.emplace_back(client_worker, host, port, workload_op);
    }

    // --- Run Test Duration ---
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

    // --- Stop Test ---
    keep_running.store(false);
    std::cout << "⏳ Stopping test and joining threads...\n";

    for (auto& t : threads) {
        t.join();
    }

    // --- Report Results ---
    long long final_requests = total_requests.load();
    long long final_errors = total_errors.load();
    double rps = static_cast<double>(final_requests) / duration_sec;

    std::cout << "\n--- Test Complete ---\n"
              << "Total Requests: " << final_requests << "\n"
              << "Total Errors:   " << final_errors << "\n"
              << "Duration:       " << duration_sec << " s\n"
              << "Throughput:     " << std::fixed << rps << " req/s\n";

    return 0;
}