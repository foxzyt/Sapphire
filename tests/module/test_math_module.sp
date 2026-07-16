// Test: Math module functions
function main() void {
    var a = Math.abs(-5);
    if (a != 5) { print("FAIL: Math.abs"); return; }
    
    var b = Math.pow(2, 3);
    if (b != 8) { print("FAIL: Math.pow"); return; }
    
    var c = Math.sqrt(9);
    if (c != 3) { print("FAIL: Math.sqrt"); return; }
    
    var d = Math.max(10, 20);
    if (d != 20) { print("FAIL: Math.max"); return; }
    
    var e = Math.min(10, 20);
    if (e != 10) { print("FAIL: Math.min"); return; }
    
    print("Math module tests passed.");
}
main();