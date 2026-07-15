// Test f-strings
var name = "Sapphire";
var version = "v1.0.9";
print f"Welcome to {name} {version}!";

// Test Arrow Functions
var square = (x) => x * x;
print "Square of 5 is: " + square(5);

var add = (a, b) => a + b;
print "5 + 3 is: " + add(5, 3);

// Test Optional Chaining
class User {
    var profile;
}
class Profile {
    var email;
}

var u = User();
print "User email (uninitialized): " + u?.profile?.email; // Should not throw

// Test Nullish Coalescing
var a = nil;
var b = "Default";
print "a ?? b is: " + (a ?? b);
print "a ?? 'Other' is: " + (a ?? "Other");
var c = "Set";
print "c ?? b is: " + (c ?? b);

// Test Array Spread
var arr1 = [1, 2, 3];
var arr2 = [...arr1, 4, 5];
print "Arr2[0]: " + arr2[0];
print "Arr2[3]: " + arr2[3];

// Test Destructuring
var arr3 = [10, 20];
var [x, y] = arr3;
print "x: " + x;
print "y: " + y;

// Test Shorthand Maps
var theme = "dark";
var fontSize = 14;
var config = { theme, fontSize };
print "config.theme: " + config["theme"];

print "All tests completed.";
