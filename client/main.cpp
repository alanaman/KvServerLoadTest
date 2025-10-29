#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <stdexcept>
#include <memory>    // Required for std::unique_ptr
#include <iomanip>   // Required for std::setprecision

// Single-header client/server library
// Download from: https://github.com/yhirose/cpp-httplib
#include "httplib.h"

// --- Workload Keyspace Definitions ---
constexpr int KEYSPACE_SIZE = 1'000'000;
constexpr int LARGE_KEYSPACE_START = KEYSPACE_SIZE + 1;
constexpr int LARGE_KEYSPACE_END = 1'000'000'000;


// --- Global Atomic Counters & Stop Flag ---
std::atomic<bool> keep_running{false};
std::atomic<long long> total_requests{0};
std::atomic<long long> total_errors{0};
std::atomic<long long> total_duration_micros{0};


// --- Abstract Base Class for Workloads ---

/**
 * @brief Abstract interface for a workload operation.
 *
 * Each thread will receive its own clone of a workload object,
 * allowing it to maintain its own state (like random distributions)
 * without needing locks.
 */
class IWorkload {
public:
    virtual ~IWorkload() = default;

    /**
     * @brief (Optional) Pre-populates the server with required data
     * before the test starts.
     * @param cli A client for executing preparation requests.
     */
    virtual void prepare(httplib::Client& cli) {
        // Default implementation does nothing.
    }

    /**
     * @brief Executes a single workload operation (e.g., one HTTP request).
     * @param cli The HTTP client dedicated to this thread.
     * @param gen The random number generator dedicated to this thread.
     * @return The result of the HTTP operation.
     */
    virtual httplib::Result execute(httplib::Client& cli, std::mt19937& gen) = 0;

    /**
     * @brief Creates a deep copy of the workload object.
     * @return A std::unique_ptr to the new IWorkload instance.
     */
    virtual std::unique_ptr<IWorkload> clone() const = 0;
};


// --- Concrete Workload Implementations ---

/**
 * @brief Workload a: "put_all"
 * Performs random PUT requests within the main keyspace.
 * No preparation needed as it creates its own keys.
 */
class PutAllWorkload : public IWorkload {
    std::uniform_int_distribution<> dist;
public:
    PutAllWorkload() : dist(1, KEYSPACE_SIZE) {}

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int key = dist(gen);
        std::string path = "/key/" + std::to_string(key);
        std::string value = "value-" + std::to_string(key);
        return cli.Put(path.c_str(), value, "text/plain");
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<PutAllWorkload>(*this);
    }
};

/**
 * @brief Workload b: "get_all"
 * Performs random GET requests from the main keyspace.
 * Assumes keys either exist (e.g., from a previous 'put_all' run)
 * or are expected to fail (cache-miss workload).
 */
class GetAllWorkload : public IWorkload {
    std::uniform_int_distribution<> dist;
public:
    GetAllWorkload() : dist(1, KEYSPACE_SIZE) {}

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int key = dist(gen);
        std::string path = "/key/" + std::to_string(key);
        return cli.Get(path.c_str());
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<GetAllWorkload>(*this);
    }
};

/**
 * @brief Workload c: "get_popular"
 * Performs GET requests from a small, "popular" set of keys (1-100).
 * Pre-populates these keys in the prepare() step.
 */
class GetPopularWorkload : public IWorkload {
    std::uniform_int_distribution<> popular_dist;
public:
    GetPopularWorkload() : popular_dist(1, 100) {}

    void prepare(httplib::Client& cli) override {
        std::cout << "   Preparing popular keys (1-100)...\n";
        int prepared_count = 0;
        int error_count = 0;
        for (int key = 1; key <= 100; ++key) {
            std::string path = "/key/" + std::to_string(key);
            std::string value = "value-" + std::to_string(key);
            if (auto res = cli.Put(path.c_str(), value, "text/plain")) {
                if (res->status == 200) {
                    prepared_count++;
                } else {
                    error_count++;
                }
            } else {
                error_count++;
            }
        }
        std::cout << "   ... prepared " << prepared_count << " / 100 keys ("
                  << error_count << " errors).\n";
    }

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int key = popular_dist(gen);
        std::string path = "/key/" + std::to_string(key);
        return cli.Get(path.c_str());
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<GetPopularWorkload>(*this);
    }
};

/**
 * @brief Workload d: "mixed"
 * 80% "get_popular" requests, 20% "unique_put" requests.
 * Pre-populates the popular keys in the prepare() step.
 */
class MixedWorkload : public IWorkload {
    std::uniform_int_distribution<> popular_dist;
    std::uniform_int_distribution<> mix_dist;
    std::uniform_int_distribution<> unique_write_dist;
public:
    MixedWorkload() :
        popular_dist(1, 100),
        mix_dist(0, 99),
        unique_write_dist(LARGE_KEYSPACE_START, LARGE_KEYSPACE_END) {}

    void prepare(httplib::Client& cli) override {
        std::cout << "   Preparing popular keys (1-100) for 'mixed' workload...\n";
        int prepared_count = 0;
        int error_count = 0;
        for (int key = 1; key <= 100; ++key) {
            std::string path = "/key/" + std::to_string(key);
            std::string value = "value-" + std::to_string(key);
            if (auto res = cli.Put(path.c_str(), value, "text/plain")) {
                if (res->status == 200) {
                    prepared_count++;
                } else {
                    error_count++;
                }
            } else {
                error_count++;
            }
        }
        std::cout << "   ... prepared " << prepared_count << " / 100 keys ("
                  << error_count << " errors).\n";
    }

    httplib::Result execute(httplib::Client& cli, std::mt19937& gen) override {
        int op = mix_dist(gen); // 0-99

        if (op < 80) { // 80% chance: Get popular
            int key = popular_dist(gen);
            std::string path = "/key/" + std::to_string(key);
            return cli.Get(path.c_str());
        } else { // 20% chance: Put "unique" (random from large space)
            int key = unique_write_dist(gen);
            std::string path = "/key/" + std::to_string(key);
            std::string value = "value-" + std::to_string(key);
            return cli.Put(path.c_str(), value, "text/plain");
        }
    }

    std::unique_ptr<IWorkload> clone() const override {
        return std::make_unique<MixedWorkload>(*this);
    }
};


/**
 * @brief The main function for each client thread.
 *
 * @param host        Server hostname (e.g., "localhost")
 * @param port        Server port (e.g., 8080)
 * @param workload    A unique_ptr to this thread's workload object.
 * @param seed        The seed for the random number generator. If -1, use std::random_device.
 */
void client_worker(const std::string host, int port, std::unique_ptr<IWorkload> workload, int seed) {
    // Each thread gets its own persistent client and random number generator
    httplib::Client cli(host, port);
    cli.set_connection_timeout(5); // 5-second timeout

    // Seed the random number generator
    std::mt19937 gen;
    if (seed == -1) {
        // No seed provided, use random_device for true randomness
        gen.seed(std::random_device{}());
    } else {
        // A unique, deterministic seed was provided by main
        gen.seed(seed);
    }

    long long thread_requests = 0;
    long long thread_errors = 0;
    long long thread_duration_micros = 0;

    // Closed-loop: run until main thread signals to stop
    while (keep_running.load()) {
        try {
            // --- Workload Generation Logic ---
            auto start_time = std::chrono::steady_clock::now();

            httplib::Result res = workload->execute(cli, gen);

            auto end_time = std::chrono::steady_clock::now();

            // --- Response Validation ---
            if (res && res->status == 200) {
                thread_requests++;
                thread_duration_micros += std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time
                ).count();
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
    total_duration_micros.fetch_add(thread_duration_micros);
}

int main(int argc, char* argv[]) {
    if (argc != 6 && argc != 7) {
        std::cerr << "Usage: " << argv[0]
                  << " <host> <port> <threads> <duration_sec> <workload_type> [seed]\n"
                  << "Workload Types: put_all, get_all, get_popular, mixed\n"
                  << "Example: " << argv[0] << " localhost 8080 16 30 get_popular\n"
                  << "Example (fixed seed): " << argv[0] << " localhost 8080 16 30 get_all 12345\n";
        return 1;
    }

    std::string host;
    int port;
    int num_threads;
    int duration_sec;
    std::string workload_type;
    int seed = -1; // Default to -1 (no seed)
    std::unique_ptr<IWorkload> workload_template; // The template object

    try {
        host = argv[1];
        port = std::stoi(argv[2]);
        num_threads = std::stoi(argv[3]);
        duration_sec = std::stoi(argv[4]);
        workload_type = argv[5];

        if (argc == 7) {
            seed = std::stoi(argv[6]);
        }

        // --- Workload Factory ---
        // We create a single "template" object based on the workload type.
        // Each thread will get a *clone* of this template.
        if (workload_type == "put_all") {
            workload_template = std::make_unique<PutAllWorkload>();
        } else if (workload_type == "get_all") {
            workload_template = std::make_unique<GetAllWorkload>();
        } else if (workload_type == "get_popular") {
            workload_template = std::make_unique<GetPopularWorkload>();
        } else if (workload_type == "mixed") {
            workload_template = std::make_unique<MixedWorkload>();
        } else {
            throw std::invalid_argument("Invalid workload type.");
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << "\n";
        return 1;
    }

    // --- Preparation Step ---
    std::cout << "🔧 Running preparation step for workload '" << workload_type << "'...\n";
    try {
        httplib::Client prepare_cli(host, port);
        prepare_cli.set_connection_timeout(10); // 10-second timeout for prep
        workload_template->prepare(prepare_cli);
        std::cout << "✅ Preparation complete.\n\n";
    } catch (const std::exception& e) {
        std::cerr << "Error during preparation step: " << e.what() << "\n";
        std::cerr << "Please ensure server is running at http://" << host << ":" << port << "\n";
        return 1; // Exit if preparation fails
    }


    std::cout << "🚀 Starting load test...\n"
              << "   Target:    http://" << host << ":" << port << "\n"
              << "   Clients:   " << num_threads << "\n"
              << "   Duration:  " << duration_sec << " seconds\n"
              << "   Workload:  " << workload_type << " (OOP Refactor)\n";

    if (seed != -1) {
        std::cout << "   Seed:      " << seed << " (Deterministic, varied per thread)\n\n";
    } else {
        std::cout << "   Seed:      Random\n\n";
    }

    std::vector<std::thread> threads;
    keep_running.store(true);

    // --- Spawn Client Threads ---
    for (int i = 0; i < num_threads; ++i) {
        // Calculate a unique seed for each thread if a base seed is provided.
        // If no base seed (seed == -1), pass -1 to let the worker use random_device.
        int thread_seed = (seed == -1) ? -1 : (seed + i);

        // Pass a *clone* of the workload object to each thread, plus the unique seed
        threads.emplace_back(client_worker, host, port, workload_template->clone(), thread_seed);
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
    long long final_duration_micros = total_duration_micros.load();

    double rps = static_cast<double>(final_requests) / duration_sec;
    double avg_response_time_ms = 0.0;

    if (final_requests > 0) {
        avg_response_time_ms = (static_cast<double>(final_duration_micros) / 1000.0)
                             / static_cast<double>(final_requests);
    }

    std::cout << "\n--- Test Complete ---\n"
              << std::fixed << std::setprecision(2)
              << "Total Requests: " << final_requests << "\n"
              << "Total Errors:   " << final_errors << "\n"
              << "Duration:       " << duration_sec << " s\n"
              << "Throughput:     " << rps << " req/s\n"
              << "Avg. Response:  " << avg_response_time_ms << " ms\n";

    return 0;
}

