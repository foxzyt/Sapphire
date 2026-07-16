// Test: Complex for loops (C-style, for-in, foreach)
function main() void {
    // C-style for loop
    var sum = 0;
    for (var i = 0; i < 5; i = i + 1) {
        sum = sum + i;
    }
    if (sum != 10) { print("FAIL: C-style for loop sum"); return; }
    
    // For-in loop over array
    var arr = [1, 2, 3, 4, 5];
    var total = 0;
    for (var val in arr) {
        total = total + val;
    }
    if (total != 15) { print("FAIL: for-in array sum"); return; }
    
    // Nested for loops
    var matrix_sum = 0;
    for (var x = 0; x < 3; x = x + 1) {
        for (var y = 0; y < 3; y = y + 1) {
            matrix_sum = matrix_sum + (x * 3 + y);
        }
    }
    if (matrix_sum != 36) { print("FAIL: nested for loops"); return; }
    
    print("Complex for loop tests passed.");
}
main();