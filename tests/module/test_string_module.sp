// Test: String module functions
function main() void {
    var s = "   Hello World!   ";
    
    var trimmed = String.trim(s);
    if (trimmed != "Hello World!") { print("FAIL: String.trim"); return; }
    
    var upper = String.toUpperCase("hello");
    if (upper != "HELLO") { print("FAIL: String.toUpperCase"); return; }
    
    var lower = String.toLowerCase("HELLO");
    if (lower != "hello") { print("FAIL: String.toLowerCase"); return; }
    
    var len = String.length("abc");
    if (len != 3) { print("FAIL: String.length"); return; }
    
    var contains = String.contains("hello world", "world");
    if (contains != true) { print("FAIL: String.contains"); return; }
    
    var replaced = String.replace("hello world", "world", "there");
    if (replaced != "hello there") { print("FAIL: String.replace"); return; }
    
    print("String module tests passed.");
}
main();