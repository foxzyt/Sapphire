// Test script for infinitum import with version syntax
import infinitum@"1.0.0";

print("Testing infinitum import with version syntax");

// Test basic array operations
var arr = [1, 2, 3, 4, 5];
print("Original array: ");
print(arr);

// Test sum function
var total = sum(arr);
print("Sum: ");
print(total);

// Test mean function
var avg = mean(arr);
print("Mean: ");
print(avg);

print("Import test completed successfully!");
