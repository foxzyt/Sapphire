// Test: Shorthand properties in maps and class fields
function main() void {
    // Shorthand properties
    var name = "Sapphire";
    var version = 1.09;
    var config = { name, version };
    if (config["name"] != "Sapphire") { print("FAIL: shorthand name"); return; }
    if (config["version"] != 1.09) { print("FAIL: shorthand version"); return; }
    
    // Mixed shorthand and explicit
    var theme = "dark";
    var settings = { theme, fontSize: 14 };
    if (settings["theme"] != "dark") { print("FAIL: mixed shorthand"); return; }
    if (settings["fontSize"] != 14) { print("FAIL: mixed explicit"); return; }
    
    // Class fields with var/const
    class User {
        var name;
        const type = "admin";
    }
    var u = User();
    u.name = "Alice";
    if (u.name != "Alice") { print("FAIL: class var field"); return; }
    
    print("Shorthand maps and class fields tests passed.");
}
main();