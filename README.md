# Sapphire Programming Language (v1.0.8)

[![CI Build](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml/badge.svg)](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/foxzyt/Sapphire?color=blue&label=release)](https://github.com/foxzyt/Sapphire/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**[Website](https://foxzyt.github.io/Sapphire)** . **[Documentation](https://foxzyt.github.io/Sapphire/site/docs_intro.html)** . **[Avaliable Plugins](https://github.com/foxzyt/sapphire-mine)**


Sapphire is a hybrid programming language designed for performance and clarity. It combines the speed of compiled languages with the syntax of high-level scripting, making it suitable for tools, UI-driven applications, and system-level tasks.

> **Note:** Sapphire currently only supports **Windows**.

## Build and Installation

### Prerequisites
* MSVC (Microsoft Visual C++) or MinGW-w64
* CMake 3.10+

### Compilation
To compile Sapphire from source:
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Installation
Add the `build/Release` directory to your system **PATH**.

## Running Tests

Sapphire includes a test suite located in the `tests/` directory. These tests are automatically executed via GitHub Actions on every push (`.github/workflows/ci.yml`).

To run the tests manually:
```bash
sapphire tests/test_syntax.sp
sapphire tests/test_loop.sp
sapphire tests/test_list.sp
sapphire tests/test_math.sp
sapphire tests/test_types.sp
sapphire tests/test_map.sp
```

## Language Guide

### Variables and Data Types
Variables can be declared implicitly or explicitly using `var`.

```javascript
// Implicit declaration
name = "Sapphire"
version = 1.0
is_active = true

// Explicit declaration
var counter = 0
const pi = 3.1415
```

### Functions
Functions are declared using the `function` keyword and require a return type.

```javascript
function greet(name) {
    return "Hello, " + name
}

function print_message() {
    print("This function returns nothing.")
}
```

### Control Flow
Sapphire supports standard control flow structures such as `if`, `else`, `while`, and `for`.

```javascript
var limit = 10
var current = 0

while (current < limit) {
    if (current % 2 == 0) {
        print(current + " is even")
    } else {
        print(current + " is odd")
    }
    current = current + 1
}
```

### Arrays
Arrays can be created using the `[]` literal syntax and accessed via indices.

```javascript
var numbers = [1, 2, 3, 4, 5]
numbers[0] = 10
print(numbers[0]) // Outputs 10
```

### Enums
Enums can be defined using the `enum` keyword. Their values start at `0` and increment automatically.

```javascript
enum Color {
    RED,
    GREEN,
    BLUE
}

var my_color = Color.GREEN
print(my_color) // Outputs 1
```


### HashMaps (Dictionaries)
HashMaps store key-value pairs. Keys must be strings.

```javascript
var config = {
    "theme": "dark",
    "version": 1.0,
    "debug": true
}

// Accessing values
print(config["theme"])

// Modifying values
config["debug"] = false
```

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Note

Sapphire is not affiliated in anyway with SapphireFoxx or the Sapphire Language by Nithinbekal.
