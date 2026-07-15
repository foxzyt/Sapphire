## Sapphire 1.0.8: New Syntax & Examples

### 1. `switch` / `case` Statements
```cpp
var x = 2;
switch (x) {
    case 1:
        print "x is 1";
        break;
    case 2:
        print "x is 2";
        break;
    default:
        print "x is something else";
        break;
}
```

### 2. `try` / `catch` / `throw` (Exception Handling)
```cpp
try {
    print "Inside try block";
    throw "Deu ruim!";
} catch (e) {
    print "Caught an exception: ";
    print e; // "Deu ruim!"
}
```

### 3. `async` / `await` (Asynchronous Promises)
```cpp
async function taskA() string {
    print "A 1";
    await yield_point();
    return "A done";
}

var p1 = taskA();
// Event loop takes over execution
```

### 4. HashMaps (`ObjMap`) & Map Literals
```cpp
var m = {"name": "Sapphire", "version": 1.8};
m["author"] = "foxzyt";

if (m["name"] == "Sapphire") {
    print "Map works!";
}
```

### 5. Ternary Operator
```cpp
var a = 10;
var b = 20;
var max = a > b ? a : b; // 20
```

### 6. Bitwise Operations
```cpp
var a = 5; // 0101
var b = 3; // 0011

print (a & b);  // 1
print (a | b);  // 7
print (a ^ b);  // 6
print (~a);     // -6
print (a << 1); // 10
```

### 7. Loop Controls (`break` and `continue`)
```cpp
for (var k = 0; k < 10; k = k + 1) {
    if (k == 2) {
        continue; // Skips to next iteration
    }
    if (k == 4) {
        break; // Exits loop
    }
}
```
