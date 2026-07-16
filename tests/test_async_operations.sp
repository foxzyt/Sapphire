// Test: Async operations with spawn and join
function main() void {
    // Test basic spawn
    var counter = 0;
    var thread = spawn() {
        counter = counter + 1;
    };
    join(thread);
    
    if (counter != 1) { print("FAIL: spawn/join basic"); return; }
    
    // Test multiple threads
    var results = [0, 0, 0];
    var threads = [];
    
    for (var i = 0; i < 3; i = i + 1) {
        var idx = i;
        var t = spawn() {
            results[idx] = idx * 10;
        };
        listAppend(threads, t);
    }
    
    for (var j = 0; j < 3; j = j + 1) {
        join(listGet(threads, j));
    }
    
    if (results[0] != 0) { print("FAIL: async result 0"); return; }
    if (results[1] != 10) { print("FAIL: async result 10"); return; }
    if (results[2] != 20) { print("FAIL: async result 20"); return; }
    
    print("Async operations tests passed.");
}
main();