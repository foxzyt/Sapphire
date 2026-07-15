function main() {
    var sum = 0;
    for (var i = 0; i < 5; i = i + 1) {
        sum = sum + i;
    }
    
    if (sum != 10) {
        print("ERROR: For loop failed. Expected 10, got " + sum);
        System.exit(1);
    }
    
    print("For loop test passed.");
}

main();
