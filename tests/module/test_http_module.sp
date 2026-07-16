// Test: HTTP module functions
function main() void {
    // Test ping
    var pingResult = httpPing("https://google.com");
    if (pingResult != true) { print("FAIL: httpPing google.com"); return; }
    
    // Test HTTP GET
    var response = httpGet("https://jsonplaceholder.typicode.com/posts/1");
    if (response == "") { print("FAIL: httpGet empty response"); return; }
    
    print("HTTP module tests passed.");
}
main();