// ============================================================================
// SAPPHIRE COMPREHENSIVE BENCHMARK SUITE
// ============================================================================
// Tests multiple scenarios to compare Corundum (interpreter) vs Rubellite (JIT)
// Target: Rubellite should be 3x faster than Corundum
// ============================================================================

print("========================================");
print("Sapphire Comprehensive Benchmark Suite");
print("========================================");

var total_start = clock();

// ============================================================================
// BENCHMARK 1: Simple Loop Operations
// ============================================================================
print("\n--- Benchmark 1: Simple Loop Operations ---");

function bench1() {
    var sum = 0;
    for (var i = 0; i < 1000; i = i + 1) {
        sum = sum + i;
    }
}

// Warm up
for (var w = 0; w < 11; w = w + 1) {
    bench1();
}

var t0 = clock();
for (var iter = 0; iter < 1000; iter = iter + 1) {
    bench1();
}
var t1 = clock();
var corundum_1 = t1 - t0;
print("Simple Loop (Corundum): " + corundum_1 + " ms");

// ============================================================================
// BENCHMARK 2: Fibonacci (Iterative - JIT friendly)
// ============================================================================
print("\n--- Benchmark 2: Fibonacci Iterative ---");

function bench2() {
    var n = 70;
    if (n <= 1) return;
    var a = 0;
    var b = 1;
    var temp = 0;
    for (var i = 2; i <= n; i = i + 1) {
        temp = a + b;
        a = b;
        b = temp;
    }
}

// Warm up
for (var w = 0; w < 11; w = w + 1) {
    bench2();
}

t0 = clock();
for (var iter = 0; iter < 1000; iter = iter + 1) {
    bench2();
}
t1 = clock();
var corundum_2 = t1 - t0;
print("Fibonacci Iterative (Corundum): " + corundum_2 + " ms");

// ============================================================================
// BENCHMARK 3: Mathematical Operations
// ============================================================================
print("\n--- Benchmark 3: Mathematical Operations ---");

function bench3() {
    var result = 0;
    for (var i = 0; i < 100; i = i + 1) {
        result = result + (i * i) / 2.0;
        result = result - (i / 3.0);
        result = result * 1.5;
    }
}

// Warm up
for (var w = 0; w < 11; w = w + 1) {
    bench3();
}

t0 = clock();
for (var iter = 0; iter < 500; iter = iter + 1) {
    bench3();
}
t1 = clock();
var corundum_3 = t1 - t0;
print("Math Operations (Corundum): " + corundum_3 + " ms");

// ============================================================================
// BENCHMARK 4: Conditional Logic
// ============================================================================
print("\n--- Benchmark 4: Conditional Logic ---");

function bench4() {
    var result = 0;
    for (var i = 0; i < 100; i = i + 1) {
        if (i % 2 == 0) {
            result = result + i;
        } else {
            if (i % 3 == 0) {
                result = result + i * 2;
            } else {
                result = result - i;
            }
        }
    }
}

// Warm up
for (var w = 0; w < 11; w = w + 1) {
    bench4();
}

t0 = clock();
for (var iter = 0; iter < 500; iter = iter + 1) {
    bench4();
}
t1 = clock();
var corundum_4 = t1 - t0;
print("Conditional Logic (Corundum): " + corundum_4 + " ms");

// ============================================================================
// BENCHMARK 5: Nested Loops
// ============================================================================
print("\n--- Benchmark 5: Nested Loops ---");

function bench5() {
    var sum = 0;
    for (var i = 0; i < 20; i = i + 1) {
        for (var j = 0; j < 20; j = j + 1) {
            sum = sum + i * j;
        }
    }
}

// Warm up
for (var w = 0; w < 11; w = w + 1) {
    bench5();
}

t0 = clock();
for (var iter = 0; iter < 200; iter = iter + 1) {
    bench5();
}
t1 = clock();
var corundum_5 = t1 - t0;
print("Nested Loops (Corundum): " + corundum_5 + " ms");

// ============================================================================
// BENCHMARK 6: Complex Computation
// ============================================================================
print("\n--- Benchmark 6: Complex Computation ---");

function bench6() {
    var result = 0;
    for (var i = 0; i < 100; i = i + 1) {
        var x = i * 1.5;
        var y = x / 2.0;
        var z = x + y;
        if (z > 100) {
            result = result + z;
        } else {
            result = result - z;
        }
    }
}

// Warm up
for (var w = 0; w < 11; w = w + 1) {
    bench6();
}

t0 = clock();
for (var iter = 0; iter < 300; iter = iter + 1) {
    bench6();
}
t1 = clock();
var corundum_6 = t1 - t0;
print("Complex Computation (Corundum): " + corundum_6 + " ms");

// ============================================================================
// SUMMARY
// ============================================================================
print("\n========================================");
print("CORUNDUM (INTERPRETER) SUMMARY");
print("========================================");
print("Simple Loop: " + corundum_1 + " ms");
print("Fibonacci Iterative: " + corundum_2 + " ms");
print("Math Operations: " + corundum_3 + " ms");
print("Conditional Logic: " + corundum_4 + " ms");
print("Nested Loops: " + corundum_5 + " ms");
print("Complex Computation: " + corundum_6 + " ms");

var total_end = clock();
print("========================================");
print("TOTAL TIME: " + (total_end - total_start) + " ms");
print("========================================");
print("\nNOTE: Run this same benchmark with --rubellite flag");
print("to compare JIT performance vs interpreter.");
print("Target: Rubellite should be 3x faster than Corundum.");
print("========================================");
