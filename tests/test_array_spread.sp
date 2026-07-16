// Test: Array spread operator and destructuring
function main() {
    // Spread
    var arr1 = [1, 2, 3];
    var arr2 = [...arr1, 4, 5];
    if (arr2[0] != 1) { print("FAIL: spread index 0"); return; }
    if (arr2[3] != 4) { print("FAIL: spread index 3"); return; }
    if (arr2[4] != 5) { print("FAIL: spread index 4"); return; }
    
    // Multiple spreads
    var a = [1, 2];
    var b = [3, 4];
    var c = [...a, ...b];
    if (c[0] != 1) { print("FAIL: multi spread 0"); return; }
    if (c[2] != 3) { print("FAIL: multi spread 2"); return; }
    
    // Destructuring
    var [x, y] = [10, 20];
    if (x != 10) { print("FAIL: destructure x"); return; }
    if (y != 20) { print("FAIL: destructure y"); return; }
    
    var [first, second, third] = [1, 2, 3];
    if (first != 1 || second != 2 || third != 3) { print("FAIL: triple destructure"); return; }
    
    print("Test passed.");
}
main();