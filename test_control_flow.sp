function fibonacci(n) {
    if (n < 2) {
        return n;
    }
    
    var a = 0;
    var b = 1;
    var i = 1;
    while (i < n) {
        var temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return b;
}

// Warm up to trigger JIT
var j = 0;
while (j < 150) {
    fibonacci(10);
    j = j + 1;
}

// Test it
print(fibonacci(10));
