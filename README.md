<div align="center">

<img src="assets/download.svg" alt="Sapphire Logo" width="180"/>

# **Sapphire**

### A fast, lightweight and expressive hybrid programming language

[![Repo Size](https://img.shields.io/github/repo-size/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire)
[![GitHub Issues](https://img.shields.io/github/issues/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/issues)
[![GitHub Stars](https://img.shields.io/github/stars/foxzyt/Sapphire?style=social)](https://github.com/foxzyt/Sapphire/stargazers)
[![Last Commit](https://img.shields.io/github/last-commit/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/commits/main)
[![Sapphire Version](https://img.shields.io/badge/Sapphire-v1.0.6-blue)](https://github.com/foxzyt/Sapphire/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

</div>

## Development Status

Sapphire is currently under **active development**.

The language is now focused on a **grid-based rendering system**, with a spreadsheet-like architecture already functional. Current work includes:

* Making the **syntax more flexible**, including:

  * Optional semicolons
  * Automatic type inference
  * Automatic number-to-string conversion
* Language improvements and new features:

  * `for` loops
  * `enums`
  * `const`
  * Vector2D and Vector3D
  * Better error treatment
  * General syntax and usability enhancements

* What is already accomplished?

  * Vector2D and Vector2d
  * Solved the grid problem, it is working flawlessly.

*Current Development Version:** `Sapphire v1.0.7` Check out the preview of 1.0.7! It is bundled with a simple raycaster made solely with Sapphire!

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
* Build **native UI systems** easily
* Communicate with the **operating system**
* Handle **HTTP, JSON, file I/O**, and more
* Write clean, readable, and efficient code with minimal boilerplate

---

## Technologies Used

* **C++** — Core language used to build the compiler and runtime
* **CMake** — Project build system
* **SFML** — Window creation and rendering

---

## Features

* **SapphireUI**
  Native Immediate Mode GUI bundled directly with the executable

* **Arithmetic Operations**
  From basic math to advanced operations via built-in Math functions

* **Garbage Collector**
  Efficient **Mark-and-Sweep GC** implemented in C++

* **JSON Parsing**
  Native JSON parsing support

* **HTTP Library**
  Built-in HTTP features (ping, download, requests, etc.)

* **System Utilities**
  File I/O, colored terminal output, debug tools, and more

* **Simple Declarations**
  Easily declare functions, classes, and arrays (`ListUtil`)

* **Low Learning Curve**
  Create a UI window in minutes

* **Built-in Layout Engine (WIP)**
  Flexbox and grid-based layout system

* **Static Typing**
  Designed for maximum performance

* **SVM (Sapphire Virtual Machine)**
  Fast, custom-built virtual machine

* **Mine: Plugin Repository**
  Download plugins directly from the Sapphire CLI

* **Bytecode Compilation**
  Compile scripts into `.sbc` bytecode

* **Lightweight & Standalone**
  Fully statically linked executable (~30 MB, no DLLs required)

---

## Installation & Setup

### Prerequisites

Since the installer is still in development, installation is **manual**.

You may need:

* **7-Zip or WinRAR** (optional, for extraction)

### Installation Steps

1. **Download** the latest release from the repository
2. **Extract** the archive
3. Open the **Sapphire root folder**
4. Add the `build` directory to your system **PATH**
5. Open a new terminal

Run a script with:

```bash
Sapphire your_script.sp
```

This executes the script using Sapphire’s hybrid interpreter.

---

## Syntax & Examples

### UI Example

```sapphire
string btnLabel = "Click here!";
UI.CreateStyle("BlueTheme", "#1a1a2e", "#e94560", "#0f3460", 2.0, "#16213e", 10.0, "Arial", 18);

function updateUI() void {
    UI.Begin();
    UI.PushStyle("BlueTheme");

    UI.SetBGColor("#16213e");
    UI.Text("Native UI!");
    UI.Spacing();

    if (UI.Button(btnLabel, 200.0, 50.0)) {
        print "Button was clicked!";
    }

    UI.PopStyle();
    UI.End();
}

while (true) {
    updateUI();
}
```

---

### HTTP, I/O & JSON Example

*(Use Windows Terminal for ANSI color support)*

```sapphire
function main() void {
    IO.printColor("cyan", "--- Starting Integration Test ---");

    IO.printColor("yellow", "Fetching API data...");
    string url = "http://jsonplaceholder.typicode.com/todos/1";
    string response = HTTP.get(url);

    if (len(response) > 0) {
        IO.printColor("green", "HTTP Response received successfully!");

        class data = JSON.parse(response);

        IO.printColor("cyan", "Data ID:");
        print data.id;

        IO.printColor("cyan", "Title:");
        print data.title;

        string path = "backup_api.json";
        IO.printColor("yellow", "Saving backup to disk...");

        if (IO.writeFile(path, response)) {
            IO.printColor("green", "File saved: " + path);
        }

        if (IO.exists(path)) {
            IO.printColor("green", "Verification: File exists on disk.");
            string localContent = IO.readFile(path);
            IO.printColor("cyan", "Content read from file:");
            print localContent;
        }
    } else {
        IO.printColor("red", "Error: Could not connect to the API.");
    }

    IO.printColor("green", "--- Test Finished ---");
}

main();
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
