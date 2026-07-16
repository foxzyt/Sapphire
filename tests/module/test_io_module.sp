// Test: IO module functions
function main() void {
    // Write a test file
    writeFile("_test_io_temp.txt", "Hello IO!");
    
    // Check if it exists
    if (!exists("_test_io_temp.txt")) {
        print("FAIL: exists after write");
        return;
    }
    
    // Read it back
    var content = readFile("_test_io_temp.txt");
    if (content != "Hello IO!") {
        print("FAIL: readFile content mismatch");
        return;
    }
    
    // Append to it
    appendFile("_test_io_temp.txt", " Appended!");
    var appended = readFile("_test_io_temp.txt");
    if (appended != "Hello IO! Appended!") {
        print("FAIL: appendFile");
        return;
    }
    
    // Delete it
    deleteFile("_test_io_temp.txt");
    if (exists("_test_io_temp.txt")) {
        print("FAIL: deleteFile");
        return;
    }
    
    print("IO module tests passed.");
}
main();