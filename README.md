# Sapphire Programming Language (v1.0.9)

[![CI Build](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml/badge.svg)](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/foxzyt/Sapphire?color=blue&label=release)](https://github.com/foxzyt/Sapphire/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**[Website](https://foxzyt.github.io/Sapphire)** • **[Documentation](https://foxzyt.github.io/Sapphire/site/docs_intro.html)** • **[Available Plugins](https://github.com/foxzyt/sapphire-mine)**

Sapphire is a hybrid, multi-paradigm, and general-purpose **time-aware** programming language designed for speed, clarity, and developer ergonomics. It bridges the performance and low-level control of compiled languages with the clean syntax of high-level scripting, making it well-suited for desktop tools, UI-driven applications, system scripts, and embedded tasks.

<details>
<summary><b>TL;DR: Fast Facts About Sapphire (Click to expand)</b></summary>

* **Custom VM:** Sapphire runs on the **Corundum virtual machine**, a custom bytecode runtime designed and implemented completely from scratch.
* **Native JIT:** Includes **Rubellite**, an experimental JIT compilation engine generating direct x86-64 machine code (no LLVM dependencies).
* **Gemstone Toolchain:** Every tool in the ecosystem is named after minerals and gems (Beryl, Topaz, Quartz, Citrine, Garnet, Amethyst).
* **Incremental GC:** Features an **Incremental Mark-and-Sweep** garbage collector that minimizes stop-the-world pauses while ensuring automatic memory management.
* **Origins:** Sapphire was originally conceived under the working title **Mint**.
* **Ecosystem Expansion:** Standard library modules and developer utilities are actively curated and continuously expanding.
* **Cross-Platform Roadmap:** Multi-platform support for Linux and macOS is under active development, targeting the upcoming **v1.1.0 LTS** release.
</details>

> **Platform Support:** Sapphire currently runs officially on **Windows**. Cross-platform support for Linux and macOS is actively being stabilized for the v1.1.0 LTS milestone.

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

Sapphire includes a test suite located in the `tests/` directory. These tests are automatically executed via GitHub Actions on every push (`.github/workflows/ci.yml`) to ensure the language's core functionalities still work (there are more than 20 tests in the folder).

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

## Carat Toolchain

**Carat** is the unified name for the full suite of developer tools accompanying the Sapphire ecosystem:

* **Sapphire**: The primary runtime, housing both the stable **Corundum** bytecode VM and the experimental **Rubellite** JIT compiler.
* **Beryl**: The standalone executable bundler (compiles and packages `.sp` scripts into native standalone binaries).
* **Topaz**: The official package manager and runtime version manager (inspired by modern workflows like `npm` and `nvm`).
* **Citrine**: An advanced static linter featuring 200+ built-in rules, automated code fixes, and rollback support.
* **Garnet**: A lightweight, fast unit-testing runner and assertion framework for Sapphire test suites.
* **Amethyst**: An automated code formatter that keeps codebase style uniform and clean.
* **Quartz**: The official benchmarking tool with 50+ built-in suites measuring ops/sec, latency (μs), standard deviation, and GC memory allocation.

> **Development Note:** The Carat toolchain is under active development. Terminal formatting adjustments and package management refinements (such as plugin uninstallation in Topaz) are being actively stabilized ahead of the LTS release.

## License

This project is licensed under the [MIT License](LICENSE).

---

## Project Status & Development Journey

Sapphire is an independent, passionate open-source project created to explore language design, interpreter internals, and virtual machine engineering.

* **Affiliation:** Sapphire is **not** affiliated in any way with SapphireFoxx or the Sapphire Language by Nithin Bekal.
* **Maturity:** While the runtime is fast, capable, and great for studying compiler/VM architectures, it is actively evolving toward production readiness. Minor edge cases in the toolchain are being resolved as we head toward the first **Long-Term Support (LTS)** milestone in **v1.1.0**.
* **Versioning & History:** When this project began, I was learning Git and Semantic Versioning on the go. Early versions did not strictly adhere to SemVer, which led to unconventional release numbering in early iterations. Since version 1.0.8, Git workflows and release practices have been completely overhauled and standardized. Starting with v1.1.0, Sapphire will strictly follow the [SemVer 2.0.0](https://semver.org/) specification.

Feel free to study the source code, fork the repository, experiment with the grammar, and build something exciting with Sapphire!
