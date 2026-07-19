#include "garnet.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include "vm.h"
#include "object.h"
#include "value.h"
#include "preprocessor/preprocessor.h"

namespace garnet {

FileResult run_single_test_file(const std::string& filepath) {
    FileResult file_res;
    file_res.filepath = filepath;
    
    auto start_file = std::chrono::high_resolution_clock::now();
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        file_res.file_error = true;
        file_res.error_message = "Could not open file";
        return file_res;
    }
    
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    VM vm;
    vm.add_module_search_path(std::filesystem::path(filepath).parent_path().string());
    
    Preprocessor prep;
    std::string processed = prep.process(source);
    
    try {
        vm.interpret(processed);
    } catch (const std::exception& e) {
        file_res.file_error = true;
        file_res.error_message = std::string("Initialization error: ") + e.what();
        return file_res;
    }
    
    // Find all tests (global functions or class methods starting with test/should)
    std::vector<std::string> global_tests;
    std::vector<std::pair<std::string, std::string>> class_tests;
    
    for (const auto& pair : vm.globals) {
        std::string name = pair.first;
        SapphireValue val = pair.second;
        
        if (name.rfind("test", 0) == 0 || name.rfind("should", 0) == 0) {
            if (is_obj_type(val, OBJ_FUNCTION) || is_obj_type(val, OBJ_CLOSURE) || is_obj_type(val, OBJ_NATIVE)) {
                global_tests.push_back(name);
            }
        }
        
        if (name.rfind("Test", 0) == 0 || name.rfind("test", 0) == 0) {
            if (is_obj_type(val, OBJ_CLASS)) {
                ObjClass* klass = static_cast<ObjClass*>(val.as.obj);
                for (const auto& method_pair : klass->methods) {
                    std::string method_name = method_pair.first;
                    if (method_name.rfind("test", 0) == 0 || method_name.rfind("should", 0) == 0) {
                        class_tests.push_back({name, method_name});
                    }
                }
            }
        }
    }
    
    std::sort(global_tests.begin(), global_tests.end());
    std::sort(class_tests.begin(), class_tests.end());
    
    // Run global tests
    for (const auto& test_name : global_tests) {
        TestResult res;
        res.test_name = test_name;
        
        auto start_test = std::chrono::high_resolution_clock::now();
        try {
            std::string test_code = test_name + "();";
            vm.interpret(test_code);
            res.passed = true;
        } catch (const std::exception& e) {
            res.passed = false;
            res.error_message = e.what();
        }
        auto end_test = std::chrono::high_resolution_clock::now();
        res.elapsed_ms = std::chrono::duration<double, std::milli>(end_test - start_test).count();
        file_res.tests.push_back(res);
    }
    
    // Run class tests
    for (const auto& class_test : class_tests) {
        std::string class_name = class_test.first;
        std::string method_name = class_test.second;
        
        TestResult res;
        res.test_name = class_name + "." + method_name;
        
        auto start_test = std::chrono::high_resolution_clock::now();
        try {
            std::string test_code = "var test_inst = " + class_name + "(); test_inst." + method_name + "();";
            vm.interpret(test_code);
            res.passed = true;
        } catch (const std::exception& e) {
            res.passed = false;
            res.error_message = e.what();
        }
        auto end_test = std::chrono::high_resolution_clock::now();
        res.elapsed_ms = std::chrono::duration<double, std::milli>(end_test - start_test).count();
        file_res.tests.push_back(res);
    }
    
    auto end_file = std::chrono::high_resolution_clock::now();
    file_res.total_elapsed_ms = std::chrono::duration<double, std::milli>(end_file - start_file).count();
    
    return file_res;
}

void run_tests(const std::string& path) {
    std::vector<std::string> test_files;
    
    try {
        if (std::filesystem::is_directory(path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string fname = entry.path().filename().string();
                    if ((fname.rfind("test_", 0) == 0 ||
                         fname.find("_test.sp") != std::string::npos ||
                         fname.find("test.sp") != std::string::npos) &&
                        entry.path().extension() == ".sp") {
                        test_files.push_back(entry.path().string());
                    }
                }
            }
        } else {
            test_files.push_back(path);
        }
    } catch (...) {
        std::cerr << "\x1b[31m[Error] Directory/file access failed:\x1b[0m " << path << "\n";
        return;
    }
    
    if (test_files.empty()) {
        std::cout << "\x1b[33mNo test files found matching:\x1b[0m " << path << "\n";
        return;
    }
    
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::cout << "                 \x1b[1mGARNET TEST RUNNER\x1b[0m\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::cout << "Found \x1b[36m" << test_files.size() << "\x1b[0m test suite files.\n\n";
    
    std::vector<FileResult> results(test_files.size());
    std::mutex mtx;
    size_t next_idx = 0;
    
    auto worker = [&]() {
        while (true) {
            size_t idx = 0;
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (next_idx >= test_files.size()) break;
                idx = next_idx++;
            }
            FileResult res = run_single_test_file(test_files[idx]);
            {
                std::lock_guard<std::mutex> lock(mtx);
                results[idx] = res;
            }
        }
    };
    
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 2;
    if (num_threads > test_files.size()) num_threads = test_files.size();
    
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread(worker));
    }
    for (auto& t : threads) {
        t.join();
    }
    
    int total_suites = 0;
    int passed_suites = 0;
    int failed_suites = 0;
    int total_cases = 0;
    int passed_cases = 0;
    int failed_cases = 0;
    double total_duration = 0.0;
    
    for (const auto& f : results) {
        total_suites++;
        std::cout << "\x1b[1mSuite:\x1b[0m \x1b[36m" << f.filepath << "\x1b[0m (" << std::fixed << std::setprecision(2) << f.total_elapsed_ms << "ms)\n";
        total_duration += f.total_elapsed_ms;
        
        if (f.file_error) {
            std::cout << "  \x1b[31m[ERROR]\x1b[0m " << f.error_message << "\n\n";
            failed_suites++;
            continue;
        }
        
        bool suite_passed = true;
        for (const auto& t : f.tests) {
            total_cases++;
            std::cout << "  - " << t.test_name << " ... ";
            if (t.passed) {
                std::cout << "\x1b[32mPASSED\x1b[0m (" << t.elapsed_ms << "ms)\n";
                passed_cases++;
            } else {
                std::cout << "\x1b[31mFAILED\x1b[0m (" << t.elapsed_ms << "ms)\n";
                std::cout << "    \x1b[33m=> Error:\x1b[0m " << t.error_message << "\n";
                failed_cases++;
                suite_passed = false;
            }
        }
        
        if (suite_passed) {
            passed_suites++;
        } else {
            failed_suites++;
        }
        std::cout << "\n";
    }
    
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::cout << "                   \x1b[1mTEST RUN SUMMARY\x1b[0m\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::cout << "Suites: " << (failed_suites > 0 ? "\x1b[31m" : "\x1b[32m") << passed_suites << " passed\x1b[0m, " << total_suites << " total\n";
    std::cout << "Cases:  " << (failed_cases > 0 ? "\x1b[31m" : "\x1b[32m") << passed_cases << " passed\x1b[0m, " << total_cases << " total\n";
    std::cout << "Time:   " << total_duration << " ms (parallel run on " << num_threads << " threads)\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n\n";
}

} // namespace garnet
