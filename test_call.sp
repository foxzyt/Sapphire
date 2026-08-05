function add(a, b) {
    return a + b;
}

print(add(5, 7));

function fibonacci(n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

var start = clock();
print("Fibonacci 30:");
print(fibonacci(30));
print("Time:");
print(clock() - start);
