// Bitwise Operators Test
var a = 5; // 0101
var b = 3; // 0011

print(a & b);  // 1 (0001)
print(a | b);  // 7 (0111)
print(a ^ b);  // 6 (0110)
print(~a);     // -6
print(a << 1); // 10 (1010)
print(a >> 1); // 2 (0010)

// Switch Statement Test
var value = 2;

print("Switch fallthrough test:");
switch (value) {
    case 1:
        print("One");
        break;
    case 2:
        print("Two");
        // Intentional fallthrough
    case 3:
        print("Three (fell through)");
        break;
    case 4:
        print("Four");
        break;
    default:
        print("Default");
}

print("Switch default test:");
switch (10) {
    case 1:
        print("No");
        break;
    default:
        print("Yes (default)");
}
