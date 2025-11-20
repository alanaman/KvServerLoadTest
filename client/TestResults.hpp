#pragma once
#include <string>

struct TestResult {
    int threads;
    std::string workload_type;
    int duration_sec;
    long long requests;
    long long errors;
    double throughput;
    double avg_response_ms;
    // New averaged system metrics collected during the test
    double avg_cpu_percent;        // average CPU utilization (%) during test
    double avg_disk_read_kbps;     // average disk read KB/s during test
    double avg_disk_write_kbps;    // average disk write KB/s during test
};