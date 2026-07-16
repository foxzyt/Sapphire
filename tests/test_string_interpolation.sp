// Test: String interpolation (f-strings)
function main() void {
    var name = "World";
    var greeting = f"Hello {name}!";
    if (greeting != "Hello World!") { print("FAIL: basic f-string"); return; }
    
    var a = 10;
    var b = 20;
    var sum = f"{a} + {b} = {a + b}";
    if (sum != "10 + 20 = 30") { print("FAIL: f-string expression"); return; }
    
    var nested = f"Value: {f"inner {a}"}";
    print("String interpolation tests passed.");
}
main();