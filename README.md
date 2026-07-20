# Sapphire Programming Language (v1.0.9)

[![CI Build](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml/badge.svg)](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/foxzyt/Sapphire?color=blue&label=release)](https://github.com/foxzyt/Sapphire/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**[Website](https://foxzyt.github.io/Sapphire)** . **[Documentation](https://foxzyt.github.io/Sapphire/site/docs_intro.html)** . **[Avaliable Plugins](https://github.com/foxzyt/sapphire-mine)**


Sapphire is a hybrid, **time-aware** programming language designed for performance and clarity. It combines the speed of compiled languages with the syntax of high-level scripting, making it suitable for tools, UI-driven applications, and system-level tasks. 

Sapphire is designed to understand and work together with **physical time** at the core virtual machine level, offering first-class primitives for real-time execution bounds (within/fallback), temporal loops (every), bytecode-level retroactive rollbacks (try/undo), and automatic memory decay (fade).

<details>
<summary><b>TL;DR: Some more info about Sapphire (Click to expand)</b></summary>

* Sapphire runs on top of the **Corundum virtual machine**, custom-made and built from the ground up by me with NO technical assistance from AI whatsoever.
* I am building a JIT-based virtual machine with NO LLVM, just raw Assembly, called **Rubellite**, and it will be released in version 1.0.9.
* Did you know? Sapphire's entire toolchain names are based on **gems and minerals**, such as Beryl, Topaz, Quartz, etc.
* Did you know that Sapphire has an **Incremental Mark-and-Sweep** garbage collector that minimizes the "stop-the-world" effects while taking the burden of memory management off the user.
* FYI: **Generative AI** was used when expanding Sapphire's standard library and some parts of the toolchain.
* Did you know that Sapphire was initially supposed to be called **Mint**?
* Multiplatform support IS being added to Sapphire and I hope I can make it fully work in other OSes by the time I release 1.1.0 (hopefully 1.0.9, but I'll try).
</details>

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

### Benchmark Comparison

Performance results based on the best recorded execution times for Sapphire (v1.0.9) compared to standard benchmarks in similar environments.

| Language / Runtime | Recursion Time (Fib 30) | Loop Time (1M iterations) |
| :--- | :--- | :--- |
| **Sapphire** | **~0.160s - 0.170s** | **~0.037s - 0.038s** |
| **CPython 3.12** | ~0.350s - 0.450s | ~0.055s - 0.075s |
| **PyPy 7.3 (JIT)** | ~0.080s - 0.120s | ~0.015s - 0.025s |
| **Lua 5.4** | ~0.180s - 0.220s | ~0.020s - 0.030s |
| **Ruby 3.3 (CRuby)** | ~0.500s - 0.700s | ~0.040s - 0.060s |
| **PHP 8.3 (JIT)** | ~0.200s - 0.250s | ~0.020s - 0.030s |
| **JavaScript (Node.js V8)** | ~0.050s - 0.080s | ~0.003s - 0.005s |
| **Perl 5.38** | ~0.400s - 0.500s | ~0.035s - 0.045s |
| **Tcl 8.6** | ~1.200s - 1.500s | ~0.080s - 0.100s |
| **R 4.3** | ~0.800s - 1.000s | ~0.050s - 0.070s |

*Note: Sapphire results reflect the fastest recorded times from the provided test data. Variations in runtime may occur due to OS background processes and environment overhead. Variations in the other languages might also occur due to the value being an estimate.*

## Language Guide

### Variables and Data Types
Variables can be declared implicitly or explicitly using `var`. Sapphire also supports `const` for immutable variables.

```javascript
// Implicit declaration
name = "Sapphire"
version = 1.0
is_active = true

// Explicit declaration
var counter = 0
const pi = 3.1415
```

### Nullish Coalescing & Optional Chaining
Safely handle default values and potential `nil` references.

```javascript
var input = nil
var username = input ?? "Guest" // username becomes "Guest"

var user = nil
var email = user?.profile?.email // Safely returns nil instead of crashing!
```

### Functions & Arrow Functions
Standard functions can take arguments and return values. For short expressions, concise arrow functions can be used.

```javascript
// Standard function
function greet(name) {
    return "Hello, " + name
}

// Arrow function (concise)
var square = (x) => x * x
print(square(5)) // Outputs 25
```

### Classes & Object-Oriented Programming (OOP)
Sapphire supports object-oriented paradigms with classes, inheritance, and constructors.

```javascript
class Animal {
    function init(name) {
        this.name = name
    }
    
    function speak() {
        print(this.name + " makes a sound.")
    }
}

class Dog extends Animal {
    function speak() {
        print(this.name + " barks! 🐶")
    }
}

var my_dog = Dog("Rex")
my_dog.speak() // Outputs: Rex barks! 🐶
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

### Time-Aware & Retroactive Control Flow
Sapphire is uniquely time-aware, offering native primitives for managing real-time physics and state rollback.

```javascript
// 1. Time-Bound Execution (within / fallback)
within (15ms) {
    var solution = calculateComplexPathfinding()
    applyPhysics(solution)
} fallback {
    // Runs instantly if the within block takes more than 15 milliseconds physical time
    applySimplifiedPhysics()
}

// 2. Temporal Loop (every)
every (1s) {} // Suspends execution for exactly 1 second (1000ms)
print("1 second elapsed!")

// 3. Retroactive Rollback (try / undo)
var balance = 100
try {
    balance = balance - 50
    if (balance < 60) {
        print("Insufficient balance, reversing transaction!")
        undo; // Natively rolls back memory mutation, restoring balance to 100!
    }
}
print(balance) // Outputs: 100

// 4. Fade
// Decays from 100 to nil over 500 milliseconds using a linear curve
fade(500ms, linear) var health = 100
print(health) // 100
every (250ms) {}
print(health) // Around 50
every (300ms) {}
print(health) // nil (expired/garbage collected!)
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
