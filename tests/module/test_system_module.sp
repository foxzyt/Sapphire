// Test: System module functions
function main() {
    var os = getOS();
    if (os == "") { print("FAIL: getOS empty"); return; }
    print("OS: " + os);
    
    var cores = getCoreCount();
    if (cores < 1) { print("FAIL: getCoreCount"); return; }
    print("Cores: " + cores);
    
    // Test clock
    var start = clock();
    sleep(100);
    var elapsed = clock() - start;
    if (elapsed < 0.05) { print("FAIL: clock/sleep timing"); return; }
    
    print("Test passed.");
}
main();