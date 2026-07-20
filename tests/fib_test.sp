var cache = lruCreate(100);

function fibonacci(n) {
    if (n <= 1) return n;
    
    // Convert n to string for cache key
    var key = n + "";
    
    if (lruHas(cache, key)) {
        return lruGet(cache, key);
    }
    
    var result = fibonacci(n - 1) + fibonacci(n - 2);
    lruPut(cache, key, result);
    return result;
}

function main() {
    print("Testing Fibonacci(35) with LRU Cache...");
    
    var startFib = clock();
    var fibResult = fibonacci(35);
    var endFib = clock();
    
    print("Fibonacci(35) = " + fibResult);
    print("Time taken: " + (endFib - startFib) + " seconds");
}

main();
