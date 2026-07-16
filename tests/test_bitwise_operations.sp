// Test: Bitwise operations and switch statements
function main() void {
    // Bitwise AND
    var a = 5 & 3;  // 0101 & 0011 = 0001
    if (a != 1) { print("FAIL: bitwise AND"); return; }
    
    // Bitwise OR
    var b = 5 | 3;  // 0101 | 0011 = 0111
    if (b != 7) { print("FAIL: bitwise OR"); return; }
    
    // Bitwise XOR
    var c = 5 ^ 3;  // 0101 ^ 0011 = 0110
    if (c != 6) { print("FAIL: bitwise XOR"); return; }
    
    // Bitwise shift
    var d = 1 << 3;
    if (d != 8) { print("FAIL: left shift"); return; }
    
    var e = 16 >> 2;
    if (e != 4) { print("FAIL: right shift"); return; }
    
    // Switch statement
    var val = 2;
    var result = "";
    switch (val) {
        case 1:
            result = "one";
            break;
        case 2:
            result = "two";
            break;
        case 3:
            result = "three";
            break;
        default:
            result = "unknown";
    }
    if (result != "two") { print("FAIL: switch case 2"); return; }
    
    // Switch with default
    var val2 = 99;
    var result2 = "";
    switch (val2) {
        case 1:
            result2 = "one";
            break;
        default:
            result2 = "default";
    }
    if (result2 != "default") { print("FAIL: switch default"); return; }
    
    print("Bitwise and switch tests passed.");
}
main();