#include "utils.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cctype>


#include <iostream>
#include <memory>
#include <array>

// Append a TestResult as a JSON object to a results file that contains a JSON array.
// If the file doesn't exist, it will be created with a single-element array.
void append_result_to_file(const TestResult& r, const std::string& path) {
    // Build JSON object string
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
     ss << "{"
         << "\"threads\": " << r.threads << ", "
         << "\"workload_type\": \"" << r.workload_type << "\", "
         << "\"duration_sec\": " << r.duration_sec << ", "
         << "\"requests\": " << r.requests << ", "
         << "\"errors\": " << r.errors << ", "
         << "\"throughput\": " << r.throughput << ", "
         << "\"avg_response_ms\": " << r.avg_response_ms << ", "
         << "\"avg_cpu_percent\": " << r.avg_cpu_percent << ", "
         << "\"avg_disk_util\": " << r.avg_perc_util << ", "
         << "\"avg_disk_write_kbps\": " << r.avg_disk_write_kbps
         << "}";

    std::string obj = ss.str();

    // Read existing file (if any)
    std::ifstream in(path);
    if (!in.good()) {
        // Create new file with array
        std::ofstream out(path, std::ios::trunc);
        out << "[" << obj << "]\n";
        return;
    }

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // Trim trailing whitespace
    while (!content.empty() && isspace(static_cast<unsigned char>(content.back()))) content.pop_back();

    if (content.empty()) {
        std::ofstream out(path, std::ios::trunc);
        out << "[" << obj << "]\n";
        return;
    }

    // If content doesn't start with '[' assume it's invalid and overwrite
    size_t first_non_ws = content.find_first_not_of(" \t\n\r");
    if (first_non_ws == std::string::npos || content[first_non_ws] != '[') {
        std::ofstream out(path, std::ios::trunc);
        out << "[" << obj << "]\n";
        return;
    }

    // Find last ']' to insert before
    size_t last_bracket = content.find_last_of(']');
    if (last_bracket == std::string::npos) {
        // malformed, overwrite
        std::ofstream out(path, std::ios::trunc);
        out << "[" << obj << "]\n";
        return;
    }

    // Determine if array is empty (i.e., [  ] or [])
    bool array_empty = true;
    for (size_t i = first_non_ws + 1; i < last_bracket; ++i) {
        if (!isspace(static_cast<unsigned char>(content[i]))) { array_empty = false; break; }
    }

    std::ofstream out(path, std::ios::trunc);
    if (array_empty) {
        out << "[" << obj << "]\n";
    } else {
        // Insert comma separator
        std::string prefix = content.substr(0, last_bracket);
        out << prefix << ",\n" << obj << "]\n";
    }
}



std::string execCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;

#if defined(_WIN32)
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
#endif

    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

