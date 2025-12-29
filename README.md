[![Repo Size](https://img.shields.io/github/repo-size/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire)
[![GitHub issues](https://img.shields.io/github/issues/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/issues)
[![GitHub Stars](https://img.shields.io/github/stars/foxzyt/Sapphire?style=social)](https://github.com/foxzyt/Sapphire/stargazers)
[![Last Commit](https://img.shields.io/github/last-commit/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/commits/main)
[![Sapphire Version](https://img.shields.io/badge/Sapphire-v1.0.6-blue)](https://github.com/foxzyt/Sapphire/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

![logo](assets/download.svg)  
# **Sapphire**


## About the Project ##
Sapphire is an hybrid programming language, developed with a focus on simplicity and clarity. The project is a hybrid language, focused in high speeds and ease of use.

## With Sapphire, you can: ##

**Calculate Expressions:** Perform complex mathematical and logical operations. 

**Declare Variables:** Manage data flexibly with static typing.

**Declare functions, classes, methods, make a UI system easily, make complex arithmetic operations, communicate with the system and much more!**

**Our goal is to provide an intuitive and robust development experience.**

**Version Currently in Development: Sapphire v1.0.7 (will have a lot of UI syntax changes and prob a layout engine :D )**

## Technologies Used ##
**C++:** The **base language** for the development of Sapphire's compiler.

**CMake:** Used to **manage the project's** build process.

**SFML** Used to **render** the windows.

## How to Install and Set Up ##
To start using Sapphire, follow the steps below:

### Prerequisites ###
Since the installer is being worked on, installation is manual.
Make sure you have the following software installed on your system:

7zip or WinRar (optional) : To extract Sapphire's files.

### Installation ###

**- Download** the **latest** release from the repository.

**- Extract the files** using Windows Explorer, 7zip or WinRar.

**- Navigate inside Sapphire's** root folder using your Windows Explorer.

**- Add the 'build'** directory in your PATH

**- Open a new terminal** in Sapphire's root directory or your project's directory.

Now you can do : **Sapphire <your_script.sp>**

This will execute your script using Sapphire's hybrid interpreter.

## Syntax and Examples ##
Sapphire supports static types, complex operations and even making an UI! **You can see the test scripts included in the latest release of Sapphire (v1.0.6)**
UI Scripts:

```
string btnLabel = "Clique Aqui";
UI.CreateStyle("BlueTheme", "#1a1a2e", "#e94560", "#0f3460", 2.0, "#16213e", 10.0, "Arial", 18);

function updateUI() void {
    UI.Begin();
    UI.PushStyle("BlueTheme");

    UI.SetBGColor("#16213e");
    UI.Text("UI Nativa!");
    UI.Spacing();

    if (UI.Button(btnLabel, 200.0, 50.0)) {
        print "Botão foi clickado!";
    }

    UI.PopStyle();
    UI.End();
}

while (true) {
    updateUI();
}
```

HTTP, I/O and JSON script:        (Run with Windows Terminals so it shows the colors, CMD dosen't supports ANSI characters)

```
function main() void {
    IO.printColor("cyan", "--- Starting Integration Test ---");

    IO.printColor("yellow", "Fetching API data...");
    string url = "http://jsonplaceholder.typicode.com/todos/1";
    string response = HTTP.get(url);

    if (len(response) > 0) {
        IO.printColor("green", "HTTP Response received successfully!");

        class data = JSON.parse(response);

        IO.printColor("cyan", "Data ID: ");
        print data.id;
        IO.printColor("cyan", "Title: ");
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

## How to Contribute ##
We appreciate your interest in contributing to the Sapphire project! Currently, the primary way to contribute is by reporting issues or by make pull requests.

## Reporting Issues ##
If you find a bug, have a feature suggestion, or any other question, please open a new Issue in our repository. When opening an issue, please provide as much detail as possible, including:

**- Clear description of the problem / suggestion.**

**- Steps to reproduce (if it's a bug).**

**- Expected behavior vs. observed behavior.**

**- Information about your environment (OS, compiler version, etc.).**

## Author ##
**foxzyt**

## License ##
This project is licensed under the **MIT License** - see the LICENSE file for details.
