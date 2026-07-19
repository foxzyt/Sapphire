#ifndef SPARK_COMMANDS_TEST_HPP
#define SPARK_COMMANDS_TEST_HPP

#include "core/fs_utils.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>
#include "termcolor.hpp"

namespace spark {
namespace commands {

inline std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) {
        start++;
    }
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

inline int cmd_test(const std::filesystem::path& working_dir, std::string custom_dir = "") {
    namespace fs = std::filesystem;
    
    fs::path test_dir = working_dir / (custom_dir.empty() ? "tests" : custom_dir);
    
    if (!fs::exists(test_dir) || !fs::is_directory(test_dir)) {
        std::cerr << termcolor::yellow << "No tests directory found at " << test_dir << termcolor::reset << std::endl;
        return 0;
    }

    // Parse SparkTestConfig.txt if present
    std::map<std::string, std::string> expected_outputs;
    fs::path config_path = test_dir / "SparkTestConfig.txt";
    if (fs::exists(config_path)) {
        std::ifstream cfg(config_path);
        std::string line;
        std::string current_test = "";
        while (std::getline(cfg, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            
            if (line.front() == '[' && line.back() == ']') {
                current_test = line.substr(1, line.length() - 2);
            } else if (line.find("EXPECT:") == 0 && !current_test.empty()) {
                std::string val = line.substr(7);
                expected_outputs[current_test] = trim(val);
            }
        }
    }

    std::cout << termcolor::cyan << termcolor::bold << "Running tests in " << test_dir << "..." << termcolor::reset << std::endl;
    
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failed_tests;

    fs::path cache_dir = working_dir / ".cache";
    fs::create_directories(cache_dir);
    fs::path temp_out = cache_dir / "spark_test_output.tmp";

    for (const auto& entry : fs::recursive_directory_iterator(test_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sp") {
            std::string filename = entry.path().filename().string();
            std::cout << "test " << filename << " ... ";
            
            std::string cmd = "sapphire \"" + entry.path().string() + "\" > \"" + temp_out.string() + "\" 2>&1";
            int result = std::system(cmd.c_str());
            
            bool test_passed = (result == 0);
            
            if (expected_outputs.find(filename) != expected_outputs.end()) {
                std::string expected = expected_outputs[filename];
                std::ifstream t_out(temp_out);
                std::string actual_output((std::istreambuf_iterator<char>(t_out)), std::istreambuf_iterator<char>());
                
                if (actual_output.find(expected) != std::string::npos) {
                    test_passed = true;
                } else {
                    test_passed = false;
                }
            }
            
            if (test_passed) {
                std::cout << termcolor::green << "ok" << termcolor::reset << std::endl;
                passed++;
            } else {
                std::cout << termcolor::red << "FAILED" << termcolor::reset << std::endl;
                failed++;
                failed_tests.push_back(filename);
            }
        }
    }

    try { fs::remove(temp_out); } catch(...) {}

    std::cout << "\nTest result: ";
    if (failed == 0 && passed > 0) {
        std::cout << termcolor::green << "ok" << termcolor::reset;
    } else if (failed > 0) {
        std::cout << termcolor::red << "FAILED" << termcolor::reset;
    } else {
        std::cout << termcolor::yellow << "no tests found" << termcolor::reset;
    }
    std::cout << ". " << passed << " passed; " << failed << " failed" << std::endl;
    
    if (failed > 0) {
        std::cout << termcolor::red << "\nFailures:" << termcolor::reset << std::endl;
        for (const auto& f : failed_tests) {
            std::cout << "    " << f << std::endl;
        }
        return 1;
    }

    return 0;
}

} // namespace commands
} // namespace spark

#endif // SPARK_COMMANDS_TEST_HPP
