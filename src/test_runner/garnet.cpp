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
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace garnet {

thread_local int tl_assert_count = 0;

std::string value_to_str(const SapphireValue& value) {
    if (value.type == ValType::VAL_NIL) {
        return "nil";
    } else if (value.type == ValType::VAL_BOOL) {
        return value.as.boolean ? "true" : "false";
    } else if (value.type == ValType::VAL_NUMBER) {
        double int_part;
        if (modf(value.as.number, &int_part) == 0.0) {
            return std::to_string(static_cast<long long>(value.as.number));
        } else {
            return std::to_string(value.as.number);
        }
    } else if (value.type == ValType::VAL_OBJ) {
        if (is_obj_type(value, OBJ_STRING)) {
            return static_cast<ObjString*>(value.as.obj)->chars;
        }
        return "<object>";
    }
    return "<unknown>";
}

static SapphireValue assertEquals_native(int arg_count, SapphireValue* args) {
    tl_assert_count++;
    if (arg_count < 2) {
        throw std::runtime_error("assertEquals() expects at least 2 arguments (expected, actual, [message]).");
    }
    SapphireValue expected = args[0];
    SapphireValue actual = args[1];
    if (!values_equal(expected, actual)) {
        std::string msg = "Assertion failed. Expected: [" + value_to_str(expected) + "], Actual: [" + value_to_str(actual) + "]";
        if (arg_count >= 3 && is_obj_type(args[2], OBJ_STRING)) {
            msg += " - " + std::string(static_cast<ObjString*>(args[2].as.obj)->chars);
        }
        throw std::runtime_error(msg);
    }
    return SapphireValue();
}

static SapphireValue assertNotEquals_native(int arg_count, SapphireValue* args) {
    tl_assert_count++;
    if (arg_count < 2) {
        throw std::runtime_error("assertNotEquals() expects at least 2 arguments (expected, actual, [message]).");
    }
    SapphireValue expected = args[0];
    SapphireValue actual = args[1];
    if (values_equal(expected, actual)) {
        std::string msg = "Assertion failed. Values should not be equal. Expected not: [" + value_to_str(expected) + "]";
        if (arg_count >= 3 && is_obj_type(args[2], OBJ_STRING)) {
            msg += " - " + std::string(static_cast<ObjString*>(args[2].as.obj)->chars);
        }
        throw std::runtime_error(msg);
    }
    return SapphireValue();
}

static SapphireValue assertTrue_native(int arg_count, SapphireValue* args) {
    tl_assert_count++;
    if (arg_count < 1) {
        throw std::runtime_error("assertTrue() expects at least 1 argument.");
    }
    if (is_falsey(args[0])) {
        std::string msg = "Assertion failed. Value is not true.";
        if (arg_count >= 2 && is_obj_type(args[1], OBJ_STRING)) {
            msg += " - " + std::string(static_cast<ObjString*>(args[1].as.obj)->chars);
        }
        throw std::runtime_error(msg);
    }
    return SapphireValue();
}

static SapphireValue assertFalse_native(int arg_count, SapphireValue* args) {
    tl_assert_count++;
    if (arg_count < 1) {
        throw std::runtime_error("assertFalse() expects at least 1 argument.");
    }
    if (!is_falsey(args[0])) {
        std::string msg = "Assertion failed. Value is not false.";
        if (arg_count >= 2 && is_obj_type(args[1], OBJ_STRING)) {
            msg += " - " + std::string(static_cast<ObjString*>(args[1].as.obj)->chars);
        }
        throw std::runtime_error(msg);
    }
    return SapphireValue();
}

static SapphireValue assertNull_native(int arg_count, SapphireValue* args) {
    tl_assert_count++;
    if (arg_count < 1) {
        throw std::runtime_error("assertNull() expects at least 1 argument.");
    }
    if (args[0].type != ValType::VAL_NIL) {
        std::string msg = "Assertion failed. Value is not null/nil. Expected: nil, Actual: [" + value_to_str(args[0]) + "]";
        if (arg_count >= 2 && is_obj_type(args[1], OBJ_STRING)) {
            msg += " - " + std::string(static_cast<ObjString*>(args[1].as.obj)->chars);
        }
        throw std::runtime_error(msg);
    }
    return SapphireValue();
}

static SapphireValue assertNotNull_native(int arg_count, SapphireValue* args) {
    tl_assert_count++;
    if (arg_count < 1) {
        throw std::runtime_error("assertNotNull() expects at least 1 argument.");
    }
    if (args[0].type == ValType::VAL_NIL) {
        std::string msg = "Assertion failed. Value is null/nil.";
        if (arg_count >= 2 && is_obj_type(args[1], OBJ_STRING)) {
            msg += " - " + std::string(static_cast<ObjString*>(args[1].as.obj)->chars);
        }
        throw std::runtime_error(msg);
    }
    return SapphireValue();
}

static SapphireValue assertThrows_native(int arg_count, SapphireValue* args) {
    tl_assert_count++;
    if (arg_count < 1) {
        throw std::runtime_error("assertThrows() expects a function to execute.");
    }
    SapphireValue func = args[0];
    if (!is_obj_type(func, OBJ_FUNCTION) && !is_obj_type(func, OBJ_CLOSURE) && !is_obj_type(func, OBJ_NATIVE)) {
        throw std::runtime_error("assertThrows() expects first argument to be a function/callable.");
    }
    
    VM* vm = g_current_vm;
    int saved_frame_count = vm->frame_count;
    SapphireValue* saved_stack_top = vm->stack_top;
    
    vm->push(func);
    bool threw = false;
    try {
        if (vm->call_value(func, 0)) {
            bool run_success = vm->run(vm->frame_count - 1);
            if (!run_success) {
                threw = true;
            }
        } else {
            threw = true;
        }
    } catch (...) {
        threw = true;
    }
    
    vm->frame_count = saved_frame_count;
    vm->stack_top = saved_stack_top;
    
    if (!threw) {
        std::string msg = "Assertion failed. Function was expected to throw an exception but succeeded.";
        if (arg_count >= 2 && is_obj_type(args[1], OBJ_STRING)) {
            msg += " - " + std::string(static_cast<ObjString*>(args[1].as.obj)->chars);
        }
        throw std::runtime_error(msg);
    }
    return SapphireValue();
}

void define_garnet_native(VM& vm, const std::string& name, NativeFn function) {
    ObjNative* native = new_native(&vm, function);
    native->name = new_string(&vm, name);
    vm.globals[name] = native;
}

FileResult run_single_test_file(const std::string& filepath, const GarnetConfig& config) {
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
    
    define_garnet_native(vm, "assertEquals", assertEquals_native);
    define_garnet_native(vm, "assertNotEquals", assertNotEquals_native);
    define_garnet_native(vm, "assertTrue", assertTrue_native);
    define_garnet_native(vm, "assertFalse", assertFalse_native);
    define_garnet_native(vm, "assertNull", assertNull_native);
    define_garnet_native(vm, "assertNotNull", assertNotNull_native);
    define_garnet_native(vm, "assertThrows", assertThrows_native);
    
    Preprocessor prep;
    std::string processed = prep.process(source);
    
    try {
        vm.interpret(processed);
    } catch (const std::exception& e) {
        file_res.file_error = true;
        file_res.error_message = std::string("Initialization error: ") + e.what();
        return file_res;
    }
    
    // Lifecycle Hook Resolution
    bool has_before_all = vm.globals.find("beforeAll") != vm.globals.end();
    bool has_after_all = vm.globals.find("afterAll") != vm.globals.end();
    
    // Execute beforeAll()
    if (has_before_all) {
        try {
            SapphireValue beforeAllFunc = vm.globals["beforeAll"];
            vm.push(beforeAllFunc);
            if (vm.call_value(beforeAllFunc, 0)) {
                vm.run(vm.frame_count - 1);
            }
        } catch (const std::exception& e) {
            file_res.file_error = true;
            file_res.error_message = std::string("beforeAll() failed: ") + e.what();
            return file_res;
        }
    }
    
    // Find all tests (global functions or class methods starting with test/should)
    std::vector<std::string> global_tests;
    std::vector<std::pair<std::string, std::string>> class_tests;
    
    for (const auto& pair : vm.globals) {
        std::string name = pair.first;
        SapphireValue val = pair.second;
        
        // Skip hooks
        if (name == "beforeAll" || name == "afterAll" || name == "beforeEach" || name == "afterEach") continue;
        
        // Filter support
        if (!config.filter.empty()) {
            if (name.find(config.filter) == std::string::npos) continue;
        }
        
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
                    // Filter support for methods
                    if (!config.filter.empty()) {
                        std::string full_method_name = name + "." + method_name;
                        if (full_method_name.find(config.filter) == std::string::npos) continue;
                    }
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
        
        int max_attempts = config.retries + 1;
        bool passed = false;
        std::string err_msg;
        double elapsed_accum = 0.0;
        int last_assert_run = 0;
        
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            res.retries_attempted = attempt;
            tl_assert_count = 0;
            
            auto start_test = std::chrono::high_resolution_clock::now();
            try {
                // beforeEach
                if (vm.globals.find("beforeEach") != vm.globals.end()) {
                    SapphireValue beforeEachFunc = vm.globals["beforeEach"];
                    vm.push(beforeEachFunc);
                    if (vm.call_value(beforeEachFunc, 0)) {
                        vm.run(vm.frame_count - 1);
                    }
                }
                
                std::string test_code = test_name + "();";
                vm.interpret(test_code);
                
                // afterEach
                if (vm.globals.find("afterEach") != vm.globals.end()) {
                    SapphireValue afterEachFunc = vm.globals["afterEach"];
                    vm.push(afterEachFunc);
                    if (vm.call_value(afterEachFunc, 0)) {
                        vm.run(vm.frame_count - 1);
                    }
                }
                
                passed = true;
                auto end_test = std::chrono::high_resolution_clock::now();
                elapsed_accum += std::chrono::duration<double, std::milli>(end_test - start_test).count();
                last_assert_run = tl_assert_count;
                break;
            } catch (const std::exception& e) {
                err_msg = e.what();
                auto end_test = std::chrono::high_resolution_clock::now();
                elapsed_accum += std::chrono::duration<double, std::milli>(end_test - start_test).count();
                last_assert_run = tl_assert_count;
                
                // execute afterEach on failure too to clean up
                try {
                    if (vm.globals.find("afterEach") != vm.globals.end()) {
                        SapphireValue afterEachFunc = vm.globals["afterEach"];
                        vm.push(afterEachFunc);
                        if (vm.call_value(afterEachFunc, 0)) {
                            vm.run(vm.frame_count - 1);
                        }
                    }
                } catch(...) {}
            }
        }
        
        res.passed = passed;
        res.error_message = err_msg;
        res.elapsed_ms = elapsed_accum;
        res.assertions_run = last_assert_run;
        
        file_res.tests.push_back(res);
        file_res.total_assertions += last_assert_run;
    }
    
    // Run class tests
    for (const auto& class_test : class_tests) {
        std::string class_name = class_test.first;
        std::string method_name = class_test.second;
        
        TestResult res;
        res.test_name = class_name + "." + method_name;
        
        int max_attempts = config.retries + 1;
        bool passed = false;
        std::string err_msg;
        double elapsed_accum = 0.0;
        int last_assert_run = 0;
        
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            res.retries_attempted = attempt;
            tl_assert_count = 0;
            
            auto start_test = std::chrono::high_resolution_clock::now();
            try {
                // beforeEach
                if (vm.globals.find("beforeEach") != vm.globals.end()) {
                    SapphireValue beforeEachFunc = vm.globals["beforeEach"];
                    vm.push(beforeEachFunc);
                    if (vm.call_value(beforeEachFunc, 0)) {
                        vm.run(vm.frame_count - 1);
                    }
                }
                
                std::string test_code = "var test_inst = " + class_name + "(); test_inst." + method_name + "();";
                vm.interpret(test_code);
                
                // afterEach
                if (vm.globals.find("afterEach") != vm.globals.end()) {
                    SapphireValue afterEachFunc = vm.globals["afterEach"];
                    vm.push(afterEachFunc);
                    if (vm.call_value(afterEachFunc, 0)) {
                        vm.run(vm.frame_count - 1);
                    }
                }
                
                passed = true;
                auto end_test = std::chrono::high_resolution_clock::now();
                elapsed_accum += std::chrono::duration<double, std::milli>(end_test - start_test).count();
                last_assert_run = tl_assert_count;
                break;
            } catch (const std::exception& e) {
                err_msg = e.what();
                auto end_test = std::chrono::high_resolution_clock::now();
                elapsed_accum += std::chrono::duration<double, std::milli>(end_test - start_test).count();
                last_assert_run = tl_assert_count;
                
                try {
                    if (vm.globals.find("afterEach") != vm.globals.end()) {
                        SapphireValue afterEachFunc = vm.globals["afterEach"];
                        vm.push(afterEachFunc);
                        if (vm.call_value(afterEachFunc, 0)) {
                            vm.run(vm.frame_count - 1);
                        }
                    }
                } catch(...) {}
            }
        }
        
        res.passed = passed;
        res.error_message = err_msg;
        res.elapsed_ms = elapsed_accum;
        res.assertions_run = last_assert_run;
        
        file_res.tests.push_back(res);
        file_res.total_assertions += last_assert_run;
    }
    
    // Execute afterAll()
    if (has_after_all) {
        try {
            SapphireValue afterAllFunc = vm.globals["afterAll"];
            vm.push(afterAllFunc);
            if (vm.call_value(afterAllFunc, 0)) {
                vm.run(vm.frame_count - 1);
            }
        } catch (const std::exception& e) {
            std::cerr << "  \x1b[31m[ERROR]\x1b[0m afterAll() failed: " << e.what() << "\n";
        }
    }
    
    auto end_file = std::chrono::high_resolution_clock::now();
    file_res.total_elapsed_ms = std::chrono::duration<double, std::milli>(end_file - start_file).count();
    
    return file_res;
}

void export_json(const std::string& path, const std::vector<FileResult>& results) {
    json j = json::array();
    for (const auto& f : results) {
        json fj = {
            {"filepath", f.filepath},
            {"file_error", f.file_error},
            {"error_message", f.error_message},
            {"total_elapsed_ms", f.total_elapsed_ms},
            {"total_assertions", f.total_assertions}
        };
        json tj = json::array();
        for (const auto& t : f.tests) {
            tj.push_back({
                {"test_name", t.test_name},
                {"passed", t.passed},
                {"error_message", t.error_message},
                {"elapsed_ms", t.elapsed_ms},
                {"assertions_run", t.assertions_run},
                {"retries_attempted", t.retries_attempted}
            });
        }
        fj["tests"] = tj;
        j.push_back(fj);
    }
    
    std::ofstream out(path);
    if (out.is_open()) {
        out << j.dump(4);
        out.close();
        std::cout << "✨ \x1b[32mTest report exported to:\x1b[0m " << path << "\n";
    } else {
        std::cerr << "❌ \x1b[31mFailed to export JSON report to:\x1b[0m " << path << "\n";
    }
}

void run_tests(const std::string& path, const GarnetConfig& config) {
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
    std::cout << "Found \x1b[36m" << test_files.size() << "\x1b[0m test suite files.\n";
    if (!config.filter.empty()) {
        std::cout << "Filter active: \x1b[33m" << config.filter << "\x1b[0m\n";
    }
    std::cout << "\n";
    
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
            FileResult res = run_single_test_file(test_files[idx], config);
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
    int total_assertions = 0;
    double total_duration = 0.0;
    
    for (const auto& f : results) {
        total_suites++;
        std::cout << "\x1b[1mSuite:\x1b[0m \x1b[36m" << f.filepath << "\x1b[0m (" << std::fixed << std::setprecision(2) << f.total_elapsed_ms << "ms)\n";
        total_duration += f.total_elapsed_ms;
        total_assertions += f.total_assertions;
        
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
                std::cout << "\x1b[32mPASSED\x1b[0m (" << t.elapsed_ms << "ms, " << t.assertions_run << " asserts)";
                if (t.retries_attempted > 0) {
                    std::cout << " \x1b[33m(passed on retry " << t.retries_attempted << ")\x1b[0m";
                }
                std::cout << "\n";
                passed_cases++;
            } else {
                std::cout << "\x1b[31mFAILED\x1b[0m (" << t.elapsed_ms << "ms, " << t.assertions_run << " asserts)";
                if (t.retries_attempted > 0) {
                    std::cout << " \x1b[33m(failed after " << t.retries_attempted + 1 << " attempts)\x1b[0m";
                }
                std::cout << "\n";
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
    std::cout << "Suites:     " << (failed_suites > 0 ? "\x1b[31m" : "\x1b[32m") << passed_suites << " passed\x1b[0m, " << total_suites << " total\n";
    std::cout << "Cases:      " << (failed_cases > 0 ? "\x1b[31m" : "\x1b[32m") << passed_cases << " passed\x1b[0m, " << total_cases << " total\n";
    std::cout << "Assertions: \x1b[36m" << total_assertions << "\x1b[0m run successfully\n";
    std::cout << "Time:       " << total_duration << " ms (parallel run on " << num_threads << " threads)\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n\n";

    if (!config.output_path.empty()) {
        export_json(config.output_path, results);
    }
}

} // namespace garnet
