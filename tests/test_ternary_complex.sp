// Test: Complex ternary expressions
function main() {
    var a = 10;
    var b = 20;
    
    var result = (a > b) ? "greater" : "less";
    if (result != "less") { print("FAIL: basic ternary false"); return; }
    
    var result2 = (a < b) ? "greater" : "less";
    if (result2 != "greater") { print("FAIL: basic ternary true"); return; }
    
    // Nested ternary
    var x = 5;
    var category = (x > 0) ? ((x < 10) ? "small" : "large") : "negative";
    if (category != "small") { print("FAIL: nested ternary"); return; }
    
    var y = 15;
    var category2 = (y > 0) ? ((y < 10) ? "small" : "large") : "negative";
    if (category2 != "large") { print("FAIL: nested ternary large"); return; }
    
    // Ternary with function calls
    function getValue(cond) {
        return cond ? 100 : 200;
    }
    if (getValue(true) != 100) { print("FAIL: ternary in function true"); return; }
    if (getValue(false) != 200) { print("FAIL: ternary in function false"); return; }
    
    print("Test passed.");
}
main();