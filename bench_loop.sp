function test_loop() {
    var sum = 0;
    for (var i = 0; i < 10000000; i = i + 1) {
        sum = sum + 1;
    }
    return sum;
}

// Warm up JIT
test_loop();

var start = clock();
var res = test_loop();
var end = clock();

print("Loop Benchmark (10 million iterations):");
print("Result:");
print(res);
print("Time:");
print(end - start);
