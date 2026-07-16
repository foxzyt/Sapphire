// Test: Complex exception handling with try/catch/finally
function main() {
    var caught = false;
    var finallyRan = false;
    
    try {
        throw "Test error message";
    } catch (err) {
        caught = true;
        if (err != "Test error message") { print("FAIL: exception message"); return; }
    } finally {
        finallyRan = true;
    }
    
    if (!caught) { print("FAIL: catch block not executed"); return; }
    if (!finallyRan) { print("FAIL: finally block not executed"); return; }
    
    // Test that finally runs even without exception
    var finally2 = false;
    try {
        var x = 1 + 1;
    } finally {
        finally2 = true;
    }
    if (!finally2) { print("FAIL: finally without catch"); return; }
    
    print("Complex exception handling tests passed.");
}
main();