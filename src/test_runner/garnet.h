#ifndef SAPPHIRE_GARNET_H
#define SAPPHIRE_GARNET_H

#include <string>
#include <vector>

namespace garnet {

struct TestResult {
    std::string test_name;
    bool passed = false;
    std::string error_message;
    double elapsed_ms = 0.0;
};

struct FileResult {
    std::string filepath;
    bool file_error = false;
    std::string error_message;
    std::vector<TestResult> tests;
    double total_elapsed_ms = 0.0;
};

void run_tests(const std::string& path);

} // namespace garnet

#endif // SAPPHIRE_GARNET_H
