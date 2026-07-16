// Test: Advanced Math functions
function main() {
    var a = floor(3.7);
    if (a != 3) { print("FAIL: floor"); return; }
    
    var b = ceil(3.2);
    if (b != 4) { print("FAIL: ceil"); return; }
    
    var c = sin(0);
    if (c != 0) { print("FAIL: sin(0)"); return; }
    
    var d = cos(0);
    if (d != 1) { print("FAIL: cos(0)"); return; }
    
    var e = clamp(15, 0, 10);
    if (e != 10) { print("FAIL: clamp max"); return; }
    
    var f = clamp(-5, 0, 10);
    if (f != 0) { print("FAIL: clamp min"); return; }
    
    var g = clamp(5, 0, 10);
    if (g != 5) { print("FAIL: clamp mid"); return; }
    
    var h = lerp(0, 10, 0.5);
    if (h != 5) { print("FAIL: lerp"); return; }
    
    print("Test passed.");
}
main();