#ifndef SAPPHIRE_GARNET_H
#define SAPPHIRE_GARNET_H

#include <string>
#include <vector>

namespace garnet {

struct GarnetConfig {
    std::string filter;
    int retries = 0;
    std::string output_path;
};

struct TestResult {
    std::string test_name;
    bool passed = false;
    std::string error_message;
    double elapsed_ms = 0.0;
    int assertions_run = 0;
    int retries_attempted = 0;
};

struct FileResult {
    std::string filepath;
    bool file_error = false;
    std::string error_message;
    std::vector<TestResult> tests;
    double total_elapsed_ms = 0.0;
    int total_assertions = 0;
};

void run_tests(const std::string& path, const GarnetConfig& config);

} // namespace garnet

#endif // SAPPHIRE_GARNET_H
