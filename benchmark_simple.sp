print("========================================");
print("Sapphire VM - Benchmark Simples");
print("========================================");

var total_start = clock();

// 1. Loop Operations
var t0 = clock();
var sum = 0;
for (var i = 0; i < 100000; i = i + 1) {
    sum = sum + i;
}
var t1 = clock();
print("[1] Loop 100k iterations: " + (t1 - t0) + " ms");

// 2. Math Operations
t0 = clock();
var math_sum = 0;
for (var i = 0; i < 10000; i = i + 1) {
    math_sum = math_sum + (i * i);
}
t1 = clock();
print("[2] Math operations (10k): " + (t1 - t0) + " ms");

// 3. String Operations
t0 = clock();
var str = "test";
for (var i = 0; i < 1000; i = i + 1) {
    var len_str = len(str);
}
t1 = clock();
print("[3] String length (1000): " + (t1 - t0) + " ms");

// 4. Conditional Operations
t0 = clock();
var cond_sum = 0;
for (var i = 0; i < 10000; i = i + 1) {
    if (i % 2 == 0) {
        cond_sum = cond_sum + i;
    } else {
        cond_sum = cond_sum - i;
    }
}
t1 = clock();
print("[4] Conditional operations (10k): " + (t1 - t0) + " ms");

// 5. Function Calls
t0 = clock();
function add(a, b) {
    return a + b;
}
var func_sum = 0;
for (var i = 0; i < 1000; i = i + 1) {
    func_sum = func_sum + add(i, i);
}
t1 = clock();
print("[5] Function calls (1000): " + (t1 - t0) + " ms");

var total_end = clock();
print("========================================");
print("TEMPO TOTAL: " + (total_end - total_start) + " ms");
print("========================================");