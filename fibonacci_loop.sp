function fibonacci_fast(n) {
    if (n <= 1) return n;
    
    var a = 0;
    var b = 1;
    var temp = 0;
    
    for (var i = 2; i <= n; i = i + 1) {
        temp = a + b;
        a = b;
        b = temp;
    }
    
    return b;
}

// Warm up JIT (threshold is 10 calls)
for (var i = 0; i < 11; i = i + 1) {
    fibonacci_fast(70);
}

var start = clock();
var res = fibonacci_fast(70);
var end = clock();

print("Fibonacci Fast 70:");
print(res);
print("Time:");
print(end - start);
