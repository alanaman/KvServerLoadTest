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
#include <fstream>

#include "workloads/putall_workload.hpp"
#include "workloads/getall_workload.hpp"
#include "workloads/getpopular_workload.hpp"
#include "workloads/mixed_workload.hpp"

#include "TestResults.hpp"
#include "utils.h"
#include <fstream>


// --- Global Atomic Counters & Stop Flag ---
std::atomic<bool> keep_running{false};
std::atomic<long long> total_requests{0};
std::atomic<long long> total_errors{0};
std::atomic<long long> total_duration_micros{0};


void client_worker(const std::string host, int port, std::unique_ptr<IWorkload> workload, int seed);

// Run a single test with the given number of threads. Returns TestResult.
// Forward declaration of client_worker (defined below)
TestResult run_single_test(const std::string& host, int port, int num_threads, int duration_sec,
                           const std::string& workload_type, std::unique_ptr<IWorkload>& workload_template,
                           int seed) {
    // Reset globals
    total_requests.store(0);
    total_errors.store(0);
    total_duration_micros.store(0);

    std::vector<std::thread> threads;
    keep_running.store(true);

    // --- Monitor data ---
    std::atomic<bool> monitor_running{true};
    std::vector<double> cpu_samples;
    std::vector<double> disk_read_kbps_samples;
    std::vector<double> disk_write_kbps_samples;

    // Helper lambdas to read /proc/stat and /proc/diskstats
    auto read_cpu_totals = []() -> double {
        auto util_perc = execCommand("mpstat -P 0 1 1 | awk '$2 ~ /^[0-9]+$/ { print 100 - $12 }'");
        double cpu_usage = std::stod(util_perc);
        return cpu_usage;
    };

    auto read_disk_sectors = []() -> std::pair<unsigned long long, unsigned long long> {
        std::ifstream f("/proc/diskstats");
        if (!f.good()) return {0,0};
        std::string line;
        unsigned long long total_read_sectors = 0;
        unsigned long long total_write_sectors = 0;
        while (std::getline(f, line)) {
            std::istringstream ss(line);
            unsigned long long major, minor;
            std::string dev;
            // fields per linux docs
            unsigned long long reads_completed, reads_merged, sectors_read, ms_read;
            unsigned long long writes_completed, writes_merged, sectors_written, ms_write;
            if (!(ss >> major >> minor >> dev)) continue;
            // Skip loop devices and ram disks by name heuristics (e.g., "loop" or "ram")
            if (dev.rfind("loop", 0) == 0 || dev.rfind("ram", 0) == 0) continue;
            if (!(ss >> reads_completed >> reads_merged >> sectors_read >> ms_read
                      >> writes_completed >> writes_merged >> sectors_written >> ms_write)) {
                // older kernels may have different columns; try to continue
                continue;
            }
            total_read_sectors += sectors_read;
            total_write_sectors += sectors_written;
        }
        return {total_read_sectors, total_write_sectors};
    };

    // Start monitor thread: samples every 1 second while keep_running is true
    std::thread monitor_thread([&]() {
        // initial readings
        auto prev_disk = read_disk_sectors();
        using namespace std::chrono_literals;
        while (keep_running.load()) {
            std::this_thread::sleep_for(1s);
            auto cpu_percent = read_cpu_totals();
            auto cur_disk = read_disk_sectors();

            cpu_samples.push_back(cpu_percent);

            // Disk: compute sectors diff, convert to KB/s (assuming 512 bytes/sector)
            unsigned long long prev_read_sectors = prev_disk.first;
            unsigned long long prev_write_sectors = prev_disk.second;
            unsigned long long cur_read_sectors = cur_disk.first;
            unsigned long long cur_write_sectors = cur_disk.second;
            double read_kbps = 0.0;
            double write_kbps = 0.0;
            if (cur_read_sectors >= prev_read_sectors) {
                unsigned long long delta_sectors = cur_read_sectors - prev_read_sectors;
                // KB = sectors * 512 / 1024 = sectors * 0.5
                read_kbps = static_cast<double>(delta_sectors) * 0.5; // per 1 second
            }
            if (cur_write_sectors >= prev_write_sectors) {
                unsigned long long delta_sectors = cur_write_sectors - prev_write_sectors;
                write_kbps = static_cast<double>(delta_sectors) * 0.5;
            }
            disk_read_kbps_samples.push_back(read_kbps);
            disk_write_kbps_samples.push_back(write_kbps);

            prev_disk = cur_disk;
        }
        monitor_running.store(false);
    });

    // Spawn threads
    for (int i = 0; i < num_threads; ++i) {
        int thread_seed = (seed == -1) ? -1 : (seed + i);
        threads.emplace_back(client_worker, host, port, workload_template->clone(), thread_seed);
    }
    
    // Run for duration
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

    // Stop and join
    keep_running.store(false);
    for (auto& t : threads) t.join();

    // Wait for monitor thread to finish
    if (monitor_thread.joinable()) monitor_thread.join();

    // Collect results
    long long final_requests = total_requests.load();
    long long final_errors = total_errors.load();
    long long final_duration_micros = total_duration_micros.load();

    double rps = static_cast<double>(final_requests) / duration_sec;
    double avg_response_time_ms = 0.0;
    if (final_requests > 0) {
        avg_response_time_ms = (static_cast<double>(final_duration_micros) / 1000.0) / static_cast<double>(final_requests);
    }

    // Compute averages from monitor samples
    double avg_cpu_percent = 0.0;
    double avg_disk_read_kbps = 0.0;
    double avg_disk_write_kbps = 0.0;
    if (!cpu_samples.empty()) {
        double sum = 0.0;
        for (double v : cpu_samples) sum += v;
        avg_cpu_percent = sum / static_cast<double>(cpu_samples.size());
    }
    if (!disk_read_kbps_samples.empty()) {
        double sum = 0.0;
        for (double v : disk_read_kbps_samples) sum += v;
        avg_disk_read_kbps = sum / static_cast<double>(disk_read_kbps_samples.size());
    }
    if (!disk_write_kbps_samples.empty()) {
        double sum = 0.0;
        for (double v : disk_write_kbps_samples) sum += v;
        avg_disk_write_kbps = sum / static_cast<double>(disk_write_kbps_samples.size());
    }

    // Print summary to stdout
    std::cout << "\n--- Test Complete (" << num_threads << " threads) ---\n"
              << std::fixed << std::setprecision(2)
              << "Total Requests: " << final_requests << "\n"
              << "Total Errors:   " << final_errors << "\n"
              << "Duration:       " << duration_sec << " s\n"
              << "Throughput:     " << rps << " req/s\n"
              << "Avg. Response:  " << avg_response_time_ms << " ms\n"
              << "Avg. CPU:       " << avg_cpu_percent << " %\n"
              << "Avg. Disk R:    " << avg_disk_read_kbps << " KB/s\n"
              << "Avg. Disk W:    " << avg_disk_write_kbps << " KB/s\n";

    return TestResult{num_threads, workload_type, duration_sec, final_requests, final_errors, rps, avg_response_time_ms,
                      avg_cpu_percent, avg_disk_read_kbps, avg_disk_write_kbps};
}


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

    // std::cout<<"Client thread"<<seed<<std::endl;
    httplib::Client cli(host, port);
    cli.set_keep_alive(true);
    cli.set_tcp_nodelay(true);
    cli.set_connection_timeout(5);

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
    std::cout << "Running preparation step for workload '" << workload_type << "'...\n";
    try {
        httplib::Client prepare_cli(host, port);
        prepare_cli.set_connection_timeout(10); // 10-second timeout for prep
        workload_template->prepare(prepare_cli);
        std::cout << "Preparation complete.\n\n";
    } catch (const std::exception& e) {
        std::cerr << "Error during preparation step: " << e.what() << "\n";
        return 1;
    }


    std::cout << "Starting load test...\n"
              << "   Target:    http://" << host << ":" << port << "\n"
              << "   Clients:   " << num_threads << "\n"
              << "   Duration:  " << duration_sec << " seconds\n"
              << "   Workload:  " << workload_type << "\n";

    if (seed != -1) {
        std::cout << "   Seed:      " << seed << " (Deterministic, varied per thread)\n\n";
    } else {
        std::cout << "   Seed:      Random\n\n";
    }

    // Loop from 1 to num_threads and run incremental tests
    std::string results_path = "results.json";
    int t=1;
    for (t = num_threads; t <= num_threads; t=t+1) {
        std::cout << "\nRunning test with " << t << " threads...\n";
        TestResult tr = run_single_test(host, port, t, duration_sec, workload_type, workload_template, seed);

        // Append test result to JSON file
        append_result_to_file(tr, results_path);
        //slepp for 2 seconds between tests
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "All tests complete. Results written to '" << results_path << "'\n";
    return 0;
}

