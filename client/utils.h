#pragma once

#include "TestResults.hpp"
#include <string>

std::string execCommand(const char* cmd);
void append_result_to_file(const TestResult& r, const std::string& path);