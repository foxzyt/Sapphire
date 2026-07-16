// Test: Optional chaining (?.) and nullish coalescing (??)
function main() void {
    // Optional chaining
    var obj = nil;
    var result = obj?.property;
    if (result != nil) { print("FAIL: optional chaining on nil"); return; }
    
    var nested = obj?.deep?.value;
    if (nested != nil) { print("FAIL: nested optional chaining"); return; }
    
    // Nullish coalescing
    var a = nil;
    var b = a ?? "default";
    if (b != "default") { print("FAIL: nullish coalescing with nil"); return; }
    
    var c = "actual";
    var d = c ?? "fallback";
    if (d != "actual") { print("FAIL: nullish coalescing with value"); return; }
    
    // Combined
    var e = obj?.name ?? "anonymous";
    if (e != "anonymous") { print("FAIL: combined optional + nullish"); return; }
    
    var f = {"name": "Sapphire"};
    var g = f?.name ?? "fallback";
    if (g != "Sapphire") { print("FAIL: optional chaining on valid obj"); return; }
    
    print("Optional chaining and nullish coalescing tests passed.");
}
main();