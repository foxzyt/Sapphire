<div align="center">

<img src="assets/download.svg" alt="Sapphire Logo" width="180"/>

# **Sapphire**

### A fast, lightweight and expressive hybrid programming language

[![Repo Size](https://img.shields.io/github/repo-size/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire)
[![GitHub Issues](https://img.shields.io/github/issues/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/issues)
[![GitHub Stars](https://img.shields.io/github/stars/foxzyt/Sapphire?style=social)](https://github.com/foxzyt/Sapphire/stargazers)
[![Last Commit](https://img.shields.io/github/last-commit/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/commits/main)
[![Sapphire Version](https://img.shields.io/badge/Sapphire-v1.0.7-blue)](https://github.com/foxzyt/Sapphire/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

</div>

## Development Status

Sapphire is currently under **active development**.

**Current Version:** `Sapphire v1.0.7`.

---

## About the Project

**Sapphire** is a **hybrid programming language** designed with a strong focus on:

* Performance
* Simplicity
* Clarity

It aims to combine the speed of compiled languages with the ease of use of high-level scripting, making it suitable for tools, UI-driven applications, and system-level tasks.

---

## What You Can Do with Sapphire

* Perform **complex mathematical and logical expressions**
* Declare **variables, functions, classes, and arrays**
* Build **native UI systems** easily using Flexbox and Grid layout systems
* Communicate with the **operating system** and run parallel workloads
* Handle **HTTP, JSON, file I/O**, and more
* Write clean, readable, and efficient code with optional semicolons, auto type inference, and minimal boilerplate

---

## Technologies Used

* **C++** — Core language used to build the compiler and runtime
* **CMake** — Project build system
* **SFML** — Window creation and rendering

---

## Features

* **SapphireUI**
  Modern, completely namespace-free declarative UI engine featuring Flexbox, Grids, Buttons, and Displays out-of-the-box.

* **Arithmetic Operations & Math Types**
  From basic math to advanced operations via built-in Math functions and native `Vec2D` and `Vec3D` structures.

* **Garbage Collector**
  Highly optimized, lightning-fast **Incremental Mark-and-Sweep GC** with automatic 500MB thread fail-safe memory ceiling.

* **JSON Parsing**
  Native, global JSON parsing (`jsonParse`) support.

* **HTTP Library**
  Built-in global HTTP features (`httpGet`, `httpPost`, `httpPing`, `httpDownload`).

* **System & File Utilities**
  Namespace-free file I/O (`readFile`, `writeFile`, `exists`), colored terminal outputs (`printColor`), and OS command execution.

* **Flexible Syntax**
  Implicit variable declaration (Python-style `x = 10`), variable shadowing (`var`), enums, consts, macros, and optional semicolons.

* **Low Learning Curve**
  Create fully-fledged UI windows and applications in minutes.

* **Built-in Layout Engine**
  Robust Flexbox and Grid layout system running natively on the UI engine.

* **SVM (Sapphire Virtual Machine)**
  Fast, custom-built virtual machine featuring native multi-threading and parallelism.

* **Mine GUI**
  A completely redesigned, visually stunning graphical CLI interface for package management.

* **Spack Package Manager**
  Re-added `spack.exe` powered by the `SpackConfig.txt` standard.

* **Bytecode Compilation**
  Compile scripts into `.sbc` bytecode.

* **Lightweight & Standalone**
  Fully statically linked executable (~30 MB, no DLLs required).

---

## Installation & Setup

### Prerequisites

Since the installer is still in development, installation is **manual**.

You may need:

* **7-Zip or WinRAR** (optional, for extraction)

### Installation Steps

1. **Download** the latest release from the repository.
2. **Extract** the archive.
3. Open the **Sapphire root folder**.
4. Add the `build` directory to your system **PATH**.
5. Open a new terminal.

Initialize a new project:

```bash
sapphire init my_project
```

Or run a script directly with:

```bash
Sapphire your_script.sp
```

---

## Syntax & Examples

### UI Example (Namespace-free syntax with Flexbox)

```javascript
Style("BlueTheme", bgColor="#1a1a2e", textColor="#e94560", hoverColor="#0f3460", borderThickness=2.0, borderColor="#16213e", borderRadius=10.0, fontAlias="Arial", fontSize=18)

btnLabel = "Click here!"
counter = 0

function updateUI() bool {
    var layout = Flex(direction="column", gap=10.0, style="BlueTheme", children=[
        Text(text="Native UI!", width=200.0, height=40.0),
        Button(label=btnLabel + " (Clicks: " + counter + ")", width=200.0, height=50.0, onClick=function() {
            counter = counter + 1
            print("Button was clicked!")
        })
    ])
    
    var event = Render(layout)
    if (event != nil) { 
        event() 
    }
    return true
}
```

---

### HTTP, I/O & JSON Example (Optional semicolons & global APIs)

*(Use Windows Terminal for ANSI color support)*

```javascript
function main() void {
    printColor("cyan", "--- Starting Integration Test ---")

    printColor("yellow", "Fetching API data...")
    url = "http://jsonplaceholder.typicode.com/todos/1"
    response = httpGet(url)

    if (len(response) > 0) {
        printColor("green", "HTTP Response received successfully!")

        data = jsonParse(response)

        printColor("cyan", "Data ID:")
        print data.id

        printColor("cyan", "Title:")
        print data.title

        path = "backup_api.json"
        printColor("yellow", "Saving backup to disk...")

        if (writeFile(path, response)) {
            printColor("green", "File saved: " + path)
        }

        if (exists(path)) {
            printColor("green", "Verification: File exists on disk.")
            localContent = readFile(path)
            printColor("cyan", "Content read from file:")
            print localContent
        }
    } else {
        printColor("red", "Error: Could not connect to the API.")
    }

    printColor("green", "--- Test Finished ---")
}

main()
```

---

## Contributing

Contributions are welcome!

You can help by:

* Reporting bugs
* Suggesting features
* Submitting pull requests

### Reporting Issues

When opening an issue, please include:

* Clear description of the problem or suggestion
* Steps to reproduce (if applicable)
* Expected vs actual behavior
* Environment details (OS, compiler, etc.)

---

## Author

**foxzyt**

---

## License

This project is licensed under the **MIT License**. Please steal my code.
See the `LICENSE` file for more information.
