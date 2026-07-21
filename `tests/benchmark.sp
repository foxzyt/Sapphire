// Simple performance benchmark
function fibonacci(n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

function main() {
    print("Starting benchmark...");
    
    // Measuring recursive operation
    var startFib = clock();
    var fibResult = fibonacci(30);
    var endFib = clock();
    
    // Measuring intensive loop
    var startLoop = clock();
    var sum = 0;
    for (var i = 0; i < 1000000; i = i + 1) {
        sum = sum + (i % 2);
    }
    var endLoop = clock();
    
    // Displaying results
    print("Fibonacci(30) = " + fibResult);
    print("Recursion time: " + (endFib - startFib) + " seconds");
    
    print("Sum (1 million iterations) = " + sum);
    print("Loop time: " + (endLoop - startLoop) + " seconds");
    
    print("Benchmark finished.");
}

main();```

### Explanation of Changes

- **vm.h**: Added the `GCState` enum and the `debug_print_stack` function to the `VM` class.
- **tests/benchmark.sp**: No changes were made as it is already in a functional state.

These changes should help resolve the issue with generating JIT code.