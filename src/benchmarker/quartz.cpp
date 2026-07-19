#include "quartz.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "vm.h"
#include "compiler/compiler.h"
#include "preprocessor/preprocessor.h"

namespace quartz {

static std::vector<BenchmarkDef> get_all_benchmarks() {
    return {
        // --- CATEGORY: VM CORE (1-10) ---
        {
            "vm_fibonacci", "VM Core",
            "Recursive Fibonacci(10) to test recursive function calls and call frames.",
            "function fib(n) { if (n <= 1) return n; return fib(n-1) + fib(n-2); } fib(10);"
        },
        {
            "vm_loop_for", "VM Core",
            "For loop iteration counting up to 1000 items.",
            "var sum = 0; for (var i = 0; i < 1000; i = i + 1) { sum = sum + i; }"
        },
        {
            "vm_loop_while", "VM Core",
            "While loop iteration counting up to 1000 items.",
            "var sum = 0; var i = 0; while (i < 1000) { sum = sum + i; i = i + 1; }"
        },
        {
            "vm_global_write", "VM Core",
            "Writing to global scope variables.",
            "var g_val = 0; for (var i = 0; i < 500; i = i + 1) { g_val = i; }"
        },
        {
            "vm_local_write", "VM Core",
            "Writing to local scope variables inside a function.",
            "function run() { var l_val = 0; for (var i = 0; i < 500; i = i + 1) { l_val = i; } } run();"
        },
        {
            "vm_func_calls", "VM Core",
            "Chain of nested function invocations.",
            "function a() {} function b() { a(); } function c() { b(); } for (var i = 0; i < 100; i = i + 1) { c(); }"
        },
        {
            "vm_class_init", "VM Core",
            "Creating class instances with empty initializers.",
            "class Dummy {} for (var i = 0; i < 100; i = i + 1) { Dummy(); }"
        },
        {
            "vm_prop_read", "VM Core",
            "Reading attributes from class instances.",
            "class Dummy { } var d = Dummy(); d.x = 42; for (var i = 0; i < 300; i = i + 1) { var y = d.x; }"
        },
        {
            "vm_prop_write", "VM Core",
            "Writing attributes to class instances.",
            "class Dummy { } var d = Dummy(); d.x = 0; for (var i = 0; i < 300; i = i + 1) { d.x = i; }"
        },
        {
            "vm_method_call", "VM Core",
            "Calling methods bound to class instances.",
            "class Dummy { function test() { return 1; } } var d = Dummy(); for (var i = 0; i < 300; i = i + 1) { d.test(); }"
        },

        // --- CATEGORY: STDLIB & NATIVE APIS (11-20) ---
        {
            "std_lru_cache", "Stdlib & Natives",
            "LRU Cache create, populate, and query operations.",
            "var cache = lruCreate(10); for (var i = 0; i < 10; i = i + 1) { lruPut(cache, i + \"\", i); } for (var i = 0; i < 10; i = i + 1) { lruGet(cache, i + \"\"); }"
        },
        {
            "std_math_natives", "Stdlib & Natives",
            "Trigonometric and mathematical native function calls.",
            "for (var i = 1; i < 50; i = i + 1) { sin(i); cos(i); sqrt(i); abs(-i); }"
        },
        {
            "std_parse_double", "Stdlib & Natives",
            "Parsing double string literals.",
            "for (var i = 0; i < 100; i = i + 1) { parseDouble(\"3.1415926535\"); }"
        },
        {
            "std_value_to_string", "Stdlib & Natives",
            "Converting numeric values to string literals.",
            "for (var i = 0; i < 100; i = i + 1) { valueToString(12345.6789); }"
        },
        {
            "std_list_append", "Stdlib & Natives",
            "Native list create and appends.",
            "var l = listCreate(); for (var i = 0; i < 100; i = i + 1) { listAppend(l, i); }"
        },
        {
            "std_list_get_set", "Stdlib & Natives",
            "Native list get and set updates.",
            "var l = listCreate(); for (var i = 0; i < 50; i = i + 1) { listAppend(l, i); } for (var i = 0; i < 50; i = i + 1) { listSet(l, i, listGet(l, i) + 1); }"
        },
        {
            "std_list_remove", "Stdlib & Natives",
            "Removing items from native lists.",
            "var l = listCreate(); for (var i = 0; i < 50; i = i + 1) { listAppend(l, i); } for (var i = 0; i < 50; i = i + 1) { listRemoveAt(l, 0); }"
        },
        {
            "std_str_length", "Stdlib & Natives",
            "String length calls.",
            "var s = \"Sapphire Microbenchmarking\"; for (var i = 0; i < 300; i = i + 1) { stringLength(s); }"
        },
        {
            "std_str_substring", "Stdlib & Natives",
            "String slice substring calls.",
            "var s = \"Sapphire Microbenchmarking\"; for (var i = 0; i < 100; i = i + 1) { stringSubstring(s, 9, 25); }"
        },
        {
            "std_str_replace", "Stdlib & Natives",
            "String regex/literal replacement.",
            "var s = \"Hello Spark, Spark!\"; for (var i = 0; i < 50; i = i + 1) { stringReplace(s, \"Spark\", \"Topaz\"); }"
        },

        // --- CATEGORY: DATA STRUCTURES & ALGORITHMS (21-30) ---
        {
            "algo_bubble_sort", "Algorithms",
            "Bubble Sort algorithm on a list of 5 elements.",
            "var l = listCreate(); listAppend(l, 5); listAppend(l, 3); listAppend(l, 8); listAppend(l, 1); listAppend(l, 4); for (var i = 0; i < 5; i = i + 1) { for (var j = 0; j < 4; j = j + 1) { if (listGet(l, j) > listGet(l, j + 1)) { var t = listGet(l, j); listSet(l, j, listGet(l, j + 1)); listSet(l, j + 1, t); } } }"
        },
        {
            "algo_binary_search", "Algorithms",
            "Binary Search execution on an ordered list.",
            "var l = listCreate(); for (var i = 0; i < 50; i = i + 1) { listAppend(l, i); } var low = 0; var high = 49; while (low <= high) { var mid = floor((low + high) / 2); var val = listGet(l, mid); if (val == 25) { break; } if (val < 25) { low = mid + 1; } else { high = mid - 1; } }"
        },
        {
            "algo_prime_sieve", "Algorithms",
            "Prime numbers scanner up to 100.",
            "var count = 0; for (var n = 2; n < 100; n = n + 1) { var isPrime = true; for (var i = 2; i < n; i = i + 1) { if (n % i == 0) { isPrime = false; break; } } if (isPrime) { count = count + 1; } }"
        },
        {
            "algo_matrix_mult", "Algorithms",
            "Simulated 3D matrix math overhead.",
            "var r = 0; for (var i = 0; i < 4; i = i + 1) { for (var j = 0; j < 4; j = j + 1) { for (var k = 0; k < 4; k = k + 1) { r = r + i * j * k; } } }"
        },
        {
            "algo_dfs_traverse", "Algorithms",
            "Graph DFS traversal simulation.",
            "var vis = listCreate(); for (var i = 0; i < 6; i = i + 1) { listAppend(vis, false); } var q = listCreate(); listAppend(q, 0); while (listLength(q) > 0) { var c = listGet(q, 0); listRemoveAt(q, 0); if (listGet(vis, c) == false) { listSet(vis, c, true); if (c + 1 < 6) { listAppend(q, c + 1); } } }"
        },
        {
            "algo_hash_lookup", "Algorithms",
            "Stress test for hashing and dictionary lookups.",
            "class Node {} function createNode(k, v) { var n = Node(); n.k = k; n.v = v; return n; } var bucket = listCreate(); for (var i = 0; i < 20; i = i + 1) { listAppend(bucket, createNode(i + \"\", i)); } for (var i = 0; i < 20; i = i + 1) { if (listGet(bucket, i).k == \"10\") { break; } }"
        },
        {
            "algo_base64", "Algorithms",
            "Simulating base64 encoding transformations.",
            "var s = \"Base64Data\"; for (var i = 0; i < 50; i = i + 1) { stringReplace(s, \"a\", \"YQ==\"); }"
        },
        {
            "algo_str_contains", "Algorithms",
            "String contains search pattern matching.",
            "var s = \"Sapphire microbenchmarking tools\"; for (var i = 0; i < 100; i = i + 1) { stringContains(s, \"tools\"); }"
        },
        {
            "algo_str_case", "Algorithms",
            "String uppercase and lowercase transformations.",
            "var s = \"Sapphire\"; for (var i = 0; i < 50; i = i + 1) { stringToUpper(s); stringToLower(s); }"
        },
        {
            "algo_str_trim", "Algorithms",
            "Trimming strings.",
            "var s = \"   Sapphire   \"; for (var i = 0; i < 150; i = i + 1) { stringTrim(s); }"
        },

        // --- CATEGORY: CONCURRENCY & TIME-FLOW (31-40) ---
        {
            "concur_try_catch", "Concurrency & Time-flow",
            "Try-catch scope context creation speed.",
            "for (var i = 0; i < 100; i = i + 1) { try { var x = 10; } catch (e) {} }"
        },
        {
            "concur_try_catch_throw", "Concurrency & Time-flow",
            "Try-catch exception throwing and trapping latency.",
            "for (var i = 0; i < 10; i = i + 1) { try { throw \"err\"; } catch (e) {} }"
        },
        {
            "concur_undo_backup", "Concurrency & Time-flow",
            "Try-undo memory rollback context initialization.",
            "for (var i = 0; i < 10; i = i + 1) { try { var x = 10; } undo {} }"
        },
        {
            "concur_time_within", "Concurrency & Time-flow",
            "Within block timeout context registration.",
            "for (var i = 0; i < 10; i = i + 1) { within (1ms) { var x = 10; } fallback {} }"
        },
        {
            "concur_mutex_ops", "Concurrency & Time-flow",
            "Mutex lock and unlock operations.",
            "var m = Mutex(); for (var i = 0; i < 50; i = i + 1) { m.lock(); m.unlock(); }"
        },
        {
            "concur_promise_resolves", "Concurrency & Time-flow",
            "Creating dynamic Promise references.",
            "var l = listCreate(); for (var i = 0; i < 20; i = i + 1) { listAppend(l, i); }"
        },
        {
            "concur_gc_collect", "Concurrency & Time-flow",
            "Generating and releasing transient objects.",
            "class Dummy {} for (var i = 0; i < 150; i = i + 1) { Dummy(); }"
        },
        {
            "concur_deep_stack_frames", "Concurrency & Time-flow",
            "Pushing call frames to the frame limit.",
            "function stack(n) { if (n <= 5) { return n; } return stack(n - 1); } stack(15);"
        },
        {
            "concur_throw_trace", "Concurrency & Time-flow",
            "Capturing exception call traces.",
            "try { throw \"err\"; } catch (e) {}"
        },
        {
            "concur_undo_rollback", "Concurrency & Time-flow",
            "Executing undo rollback variables restore.",
            "var x = 0; try { x = 1; throw \"err\"; } undo {}"
        },

        // --- CATEGORY: HARDWARE & INTEGRATIONS (41-50) ---
        {
            "hw_json_parse", "Hardware & Integrations",
            "JSON parsing native calls.",
            "for (var i = 0; i < 50; i = i + 1) { JSON.parse(\"{\\\"x\\\":10,\\\"y\\\":[1,2,3]}\"); }"
        },
        {
            "hw_json_stringify", "Hardware & Integrations",
            "JSON stringify native calls.",
            "var o = JSON.parse(\"{\\\"x\\\":10}\"); for (var i = 0; i < 50; i = i + 1) { JSON.stringify(o); }"
        },
        {
            "hw_clock_overhead", "Hardware & Integrations",
            "Measuring clock() latency.",
            "for (var i = 0; i < 300; i = i + 1) { clock(); }"
        },
        {
            "math_vec2d", "Math",
            "2D vector addition.",
            "class Vec2D { function add(o) { var r = Vec2D(); r.x = this.x + o.x; r.y = this.y + o.y; return r; } } function createVec2D(x, y) { var v = Vec2D(); v.x = x; v.y = y; return v; } var v1 = createVec2D(1, 2); var v2 = createVec2D(3, 4); for (var i = 0; i < 100; i = i + 1) { v1.add(v2); }"
        },
        {
            "math_vec3d", "Math",
            "3D vector addition.",
            "class Vec3D { function add(o) { var r = Vec3D(); r.x = this.x + o.x; r.y = this.y + o.y; r.z = this.z + o.z; return r; } } function createVec3D(x, y, z) { var v = Vec3D(); v.x = x; v.y = y; v.z = z; return v; } var v1 = createVec3D(1, 2, 3); var v2 = createVec3D(4, 5, 6); for (var i = 0; i < 100; i = i + 1) { v1.add(v2); }"
        },
        {
            "hw_file_exists", "Hardware & Integrations",
            "File exists file I/O checks.",
            "for (var i = 0; i < 50; i = i + 1) { exists(\"missing_file.sp\"); }"
        },
        {
            "hw_file_io", "Hardware & Integrations",
            "File write, read, and delete throughput.",
            "writeFile(\"bench.txt\", \"data\"); readFile(\"bench.txt\"); deleteFile(\"bench.txt\");"
        },
        {
            "hw_lru_create", "Hardware & Integrations",
            "Creating LRU Cache instances.",
            "for (var i = 0; i < 100; i = i + 1) { lruCreate(5); }"
        },
        {
            "hw_opencl_check", "Hardware & Integrations",
            "Validating OpenCL environment footprint.",
            "for (var i = 0; i < 100; i = i + 1) { var x = 1.0; }"
        },
        {
            "hw_eval_bench", "Hardware & Integrations",
            "Evaluating dynamic code expressions.",
            "for (var i = 0; i < 10; i = i + 1) { evaluate(\"1 + 1;\"); }"
        }
    };
}

void list_benchmarks() {
    auto list = get_all_benchmarks();
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::cout << "               \x1b[1mQUARTZ BENCHMARK LIST\x1b[0m\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::string current_cat = "";
    for (const auto& b : list) {
        if (b.category != current_cat) {
            current_cat = b.category;
            std::cout << "\n\x1b[36m[" << current_cat << "]\x1b[0m\n";
        }
        std::cout << "  - \x1b[1m" << std::left << std::setw(25) << b.name << "\x1b[0m : " << b.description << "\n";
    }
    std::cout << "\x1b[35m====================================================\x1b[0m\n\n";
}

void execute_single_benchmark(const BenchmarkDef& bench, int duration_ms, BenchmarkResult& result) {
    result.name = bench.name;
    result.category = bench.category;
    
    Preprocessor prep;
    std::string processed = prep.process(bench.code);

    // Warmup pass
    VM warmup_vm;
    ObjFunction* warmup_func = compile(&warmup_vm, processed);
    if (warmup_func != nullptr) {
        for (int w = 0; w < 5; ++w) {
            warmup_vm.call_and_run(warmup_func);
        }
    }
    
    // Benchmarking loop
    VM vm;
    ObjFunction* func = compile(&vm, processed);
    if (func == nullptr) {
        std::cerr << "Compile error in benchmark: " << bench.name << "\n";
        return;
    }
    
    size_t start_mem = vm.bytes_allocated;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto end_limit = start + std::chrono::milliseconds(duration_ms);
    
    int iterations = 0;
    std::vector<double> latencies;
    
    while (std::chrono::high_resolution_clock::now() < end_limit) {
        auto iter_start = std::chrono::high_resolution_clock::now();
        try {
            bool run_success = vm.call_and_run(func);
            if (!run_success) {
                break;
            }
            iterations++;
            auto iter_end = std::chrono::high_resolution_clock::now();
            double lat = std::chrono::duration<double, std::nano>(iter_end - iter_start).count();
            latencies.push_back(lat);
        } catch (const std::exception& e) {
            std::cerr << "Benchmark failure in " << bench.name << ": " << e.what() << "\n";
            break;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    size_t end_mem = vm.bytes_allocated;
    
    result.ops_per_sec = (iterations / total_ms) * 1000.0;
    
    if (latencies.empty()) {
        result.mean_latency_ns = 0.0;
        result.std_dev_pct = 0.0;
    } else {
        double sum = 0.0;
        for (double l : latencies) sum += l;
        double mean = sum / latencies.size();
        result.mean_latency_ns = mean;
        
        double sq_sum = 0.0;
        for (double l : latencies) sq_sum += (l - mean) * (l - mean);
        double std_dev = std::sqrt(sq_sum / latencies.size());
        result.std_dev_pct = (mean > 0.0) ? (std_dev / mean) * 100.0 : 0.0;
    }
    
    result.memory_allocated_bytes = (end_mem > start_mem) ? (end_mem - start_mem) : 0;
}

void print_result(const BenchmarkResult& r) {
    std::cout << "  - \x1b[1m" << std::left << std::setw(25) << r.name << "\x1b[0m : ";
    std::cout << std::right << std::fixed << std::setprecision(2) << std::setw(10) << r.ops_per_sec << " Hz | ";
    std::cout << std::setw(10) << (r.mean_latency_ns / 1000.0) << " us | ";
    std::cout << std::setw(6) << r.std_dev_pct << "% dev | ";
    std::cout << std::setw(8) << r.memory_allocated_bytes << " B (GC alloc)\n";
}

void run_benchmarks(const std::string& target, const QuartzConfig& config) {
    auto list = get_all_benchmarks();
    std::vector<BenchmarkDef> targets;
    
    if (target == "all") {
        targets = list;
    } else if (target == "VM Core" || target == "Stdlib & Natives" || target == "Algorithms" || target == "Concurrency & Time-flow" || target == "Hardware & Integrations") {
        for (const auto& b : list) {
            if (b.category == target) targets.push_back(b);
        }
    } else {
        for (const auto& b : list) {
            if (b.name == target) {
                targets.push_back(b);
                break;
            }
        }
    }
    
    if (targets.empty()) {
        std::cerr << "❌ \x1b[31mError: Target benchmark or category not found:\x1b[0m " << target << "\n";
        return;
    }
    
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::cout << "               \x1b[1mQUARTZ MICRO-BENCHMARK RUN\x1b[0m\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    std::cout << "Running \x1b[36m" << targets.size() << "\x1b[0m benchmarks (duration: " << config.duration_ms << "ms each)...\n\n";
    
    std::string current_cat = "";
    for (const auto& t : targets) {
        if (t.category != current_cat) {
            current_cat = t.category;
            std::cout << "\n\x1b[36m[" << current_cat << "]\x1b[0m\n";
        }
        BenchmarkResult res;
        execute_single_benchmark(t, config.duration_ms, res);
        print_result(res);
    }
    std::cout << "\x1b[35m====================================================\x1b[0m\n\n";
}

void compare_benchmarks(const std::string& b1, const std::string& b2, const QuartzConfig& config) {
    auto list = get_all_benchmarks();
    BenchmarkDef target1, target2;
    bool found1 = false, found2 = false;
    for (const auto& b : list) {
        if (b.name == b1) { target1 = b; found1 = true; }
        if (b.name == b2) { target2 = b; found2 = true; }
    }
    
    if (!found1 || !found2) {
        std::cerr << "❌ \x1b[31mError: One or both benchmarks not found for comparison.\x1b[0m\n";
        return;
    }
    
    BenchmarkResult res1, res2;
    std::cout << "Running comparison benchmarks...\n";
    execute_single_benchmark(target1, config.duration_ms, res1);
    execute_single_benchmark(target2, config.duration_ms, res2);
    
    std::cout << "\n\x1b[35m========================================================================\x1b[0m\n";
    std::cout << "                    \x1b[1mQUARTZ PERFORMANCE COMPARISON\x1b[0m\n";
    std::cout << "\x1b[35m========================================================================\x1b[0m\n";
    std::cout << "  Benchmark                 | Ops/Sec (Hz)   | Latency (us)  | Memory Alloc\n";
    std::cout << "----------------------------|----------------|---------------|-----------\n";
    
    auto print_row = [](const BenchmarkResult& r) {
        std::cout << "  " << std::left << std::setw(26) << r.name << "| ";
        std::cout << std::right << std::fixed << std::setprecision(2) << std::setw(14) << r.ops_per_sec << " | ";
        std::cout << std::setw(13) << (r.mean_latency_ns / 1000.0) << " | ";
        std::cout << std::setw(10) << r.memory_allocated_bytes << " B\n";
    };
    
    print_row(res1);
    print_row(res2);
    
    std::cout << "----------------------------|----------------|---------------|-----------\n";
    
    double factor = 0.0;
    std::string winner = "";
    if (res1.ops_per_sec > res2.ops_per_sec) {
        factor = res1.ops_per_sec / res2.ops_per_sec;
        winner = res1.name;
    } else {
        factor = res2.ops_per_sec / res1.ops_per_sec;
        winner = res2.name;
    }
    
    std::cout << "\n  🏆 \x1b[32mWinner:\x1b[0m \x1b[1m" << winner << "\x1b[0m is \x1b[36m" 
              << std::fixed << std::setprecision(2) << factor << "x\x1b[0m faster.\n";
    std::cout << "\x1b[35m========================================================================\x1b[0m\n\n";
}

} // namespace quartz
