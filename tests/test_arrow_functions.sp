// Test: Arrow functions
function main() void {
    var square = (x) => x * x;
    var result = square(5);
    if (result != 25) { print("FAIL: arrow function basic"); return; }
    
    var add = (a, b) => a + b;
    if (add(3, 4) != 7) { print("FAIL: arrow function two args"); return; }
    
    var multiply = (x, y) => {
        return x * y;
    };
    if (multiply(6, 7) != 42) { print("FAIL: arrow function block body"); return; }
    
    var arr = [1, 2, 3, 4];
    var doubled = arr.map((x) => x * 2);
    // Note: map may not exist - this is just a syntax test
    print("Arrow function tests passed.");
}
main();