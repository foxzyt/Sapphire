# Sapphire Programming Language (v1.0.9)

[![CI Build](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml/badge.svg)](https://github.com/foxzyt/Sapphire/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/foxzyt/Sapphire?color=blue&label=release)](https://github.com/foxzyt/Sapphire/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**[Website](https://foxzyt.github.io/Sapphire)** . **[Documentation](https://foxzyt.github.io/Sapphire/site/docs_intro.html)** . **[Avaliable Plugins](https://github.com/foxzyt/sapphire-mine)**


Sapphire is a hybrid, multi-paradigm and general purpose, **time-aware** programming language designed for performance and clarity. It combines the speed of compiled languages and its low-level tools with the syntax of high-level scripting, making it suitable for tools, UI-driven applications, scripts, and system-level tasks. 

<details>
<summary><b>TL;DR: Some more info about Sapphire (Click to expand)</b></summary>

* Sapphire runs on top of the **Corundum virtual machine**, custom-made and built from the ground up by me with NO technical assistance from AI whatsoever.
* I am building a JIT-based virtual machine with no LLVM, just raw Assembly, called **Rubellite**, and it will be released in version 1.0.9.
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

## Carat
Carat is the official name for the Sapphire language full toolchain, which includes (as of version 1.0.9 (being developed)):

* **Sapphire**: The official runtime, which includes the Corundum and Rubellite virtual machines.
* **Beryl**: The official bundler for the language (transforms .sp scripts into .exe files).
* **Topaz**: The package manager and version manager (such as npm and nvm).
* **Citrine**: An advanced linter with 200+ hand-written lint rules. It can: fix your code, explain the errors and undo changes (if Citrine fails in correcting your code).
* **Garnet**: Simple, yet powerful test runner for Sapphire.
* **Amethyst**: A code formatter that automatically formats your code (obviously) and checks it if you want to apply the correnctions yourself.
* **Quartz**: The official benchmarker for the language, with 50 default benchmarks to measure exactly the performance of the entire language. It can compare two benchmarks, and it shows you how many Ops/Sec, latency (in microseconds), StdDev (in %) and how many bytes were allocated by the GC.

All of them **simple** to use, and very **fast**. 

> **Note:** Some errors with CMD formatting might appear, and some errors in Topaz might ALSO appear (like being unable to uninstall plugins and etc), but don't worry, I will fix it, as they are being developed and are early-stage implementations.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Note

Sapphire is **not** affiliated in anyway with SapphireFoxx or the Sapphire Language by Nithinbekal.
Sapphire is also **not** completely stable and I would advise you not to use it in serious projects, just to study about interpreters and VM implementations/prepare yourself for established languages (Sapphire is a pretty good spot to start, as it has pretty simple syntax that remembers C++, but with high-level keywords!). But it is pretty close to being finished though, as I have a deadline to release the first LTS version in 1.1.0. It is also not completely broken, just some small bugs that I have to fix (such as in Carat), although the runtime is stable!
And I have to admit some things: When I started this project, I had NO idea of what was SemVer (that's why I started at 1.0.0, but in reality I should have started at 0.1) and NO idea how to use Git. That's why I lost code, and that's why the versioning is confusing. Since 1.0.8, I've been using Git MUCH more actively and using it **right** for the first time. About the versioning- yes, I know it's crap, that's why in the first LTS version I will fix the versioning once and for all and start using the SemVer standard. I am saying this because I am not going to deceive any collaborators or anyone, really, and I want to be honest about the current shape of Sapphire.

Please steal my code, :D!
