// Test: JSON module functions
function main() void {
    // Test JSON.stringify
    var obj = {"name": "Sapphire", "version": 1.09};
    var str = JSON.stringify(obj);
    if (str == "") { print("FAIL: JSON.stringify empty"); return; }
    if (String.contains(str, "Sapphire") != true) { print("FAIL: JSON.stringify content"); return; }
    
    // Test JSON.parse
    var jsonStr = '{"name":"Test","value":42}';
    var parsed = JSON.parse(jsonStr);
    if (parsed.name != "Test") { print("FAIL: JSON.parse name"); return; }
    if (parsed.value != 42) { print("FAIL: JSON.parse value"); return; }
    
    // Test roundtrip
    var original = {"user": {"name": "Alice", "score": 100}, "active": true};
    var raw = JSON.stringify(original);
    var back = JSON.parse(raw);
    if (back.user.name != "Alice") { print("FAIL: JSON nested roundtrip"); return; }
    if (back.active != true) { print("FAIL: JSON bool roundtrip"); return; }
    
    print("JSON module tests passed.");
}
main();