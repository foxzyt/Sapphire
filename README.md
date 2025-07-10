
[![Repo Size](https://img.shields.io/github/repo-size/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire)
[![GitHub issues](https://img.shields.io/github/issues/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/issues)
[![GitHub Stars](https://img.shields.io/github/stars/foxzyt/Sapphire?style=social)](https://github.com/foxzyt/Sapphire/stargazers)
[![Last Commit](https://img.shields.io/github/last-commit/foxzyt/Sapphire)](https://github.com/foxzyt/Sapphire/commits/main)
[![Sapphire Version](https://img.shields.io/badge/Sapphire-v1.0.3a-blue)](https://github.com/foxzyt/Sapphire/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# **Sapphire 💎**

## About the Project ##
### Sapphire is an compiled programming language, developed with a focus on simplicity and clarity. Currently in its interpretation phase, the project is evolving to become a compiled language, aiming for higher performance and optimization. ###

## With Sapphire, you can: ##

### Calculate Expressions: Perform complex mathematical and logical operations. ###

### Declare Variables: Manage data flexibly with static typing. ###

### Our goal is to provide an intuitive and robust development experience. ###

## Technologies Used ##
### C++: The base language for the development of Sapphire's compiler. ###

### CMake: Used to manage the project's build process. ###

## How to Install and Set Up ##
### To start using Sapphire, follow the steps below: ###

## Prerequisites ✅ ##
### Make sure you have the following software installed on your system: ###

### Git: To clone the repository. ###

### CMake: To configure the build environment. ###

### A C++ compiler: (e.g., GCC, Clang, MSVC) Compatible with C++17 or newer. ###

## Installation ##
## Clone the repository: ##
### Open your terminal or command prompt and execute the following command: ###

### git clone https://github.com/foxzyt/Sapphire.git ###

## Navigate to the project directory: ##

### cd Sapphire ###

## Create the build directory and compile the project: ##

### mkdir build ###
### cd build ###
### cmake .. ###
### cmake --build . ###

### This will compile the sapphire.exe (or sapphire on Unix-like systems) executable inside the build folder. ###

## How to Use 📖 ##
### After installation, you can execute your Sapphire scripts: ###

### Navigate to the root folder of the Sapphire project (where you cloned the repository). ###

### Execute a Sapphire script: ###

### .\build\sapphire.exe [your_sapphire_script_name.sapphire] ###

### Replace [your_sapphire_script_name.sapphire] with the path and name of your Sapphire script file. ###

### Example: ###

### .\build\sapphire.exe examples/my_first_script.sapphire ###

## Syntax and Examples ✏️ ##
### Sapphire supports static types and basic operations. Below is an example script demonstrating the core functionalities: ###

// ===============================================

//  Sapphire Test Script with Static Types

// ===============================================

print "--- Starting Static Type Test ---";

print "";

// --- 1. Valid Declarations ---

print "--- 1. Testing Valid Declarations ---";

int    my_integer = 100;

double my_double  = 50.5;

string my_text    = "Hello, Sapphire!";

bool   is_true    = true;

bool   is_false   = false;

print "Static variables declared successfully!";

print my_integer;

print my_double;

print my_text;

print is_true;

print "";


// --- 2. Valid Operations ---

print "--- 2. Testing Valid Operations ---";

int    a = 20;

int    b = 10;

double c = 2.5;

print "a - b =";

print a - b; // Should be 10

print "a * c =";

print a * c; // Should be 50.0

string s1 = "Sapphire ";

string s2 = "is awesome!";

print s1 + s2; // Should be "Sapphire is awesome!"


bool result_comp = (a > b);

print "Result of a > b:";

print result_comp; // Should be true

print "";


// --- 3. Flow Control with Static Types ---

print "--- 3. Testing Flow Control ---";

int counter = 3;

print "While loop with static variable:";

while (counter > 0) {

print counter;
counter = counter - 1;
    
}

print "End of loop.";

print "";

// --- 4. Final Stack Sanity Test ---

print "--- 4. Final Test with Native Function ---";

print "If this call works, the stack is clean and correct.";

print "Time since start (s):";

print clock();

print "";

print "----------------------------------------------------";

print "--- SUCCESS! All valid tests passed. ---";

print "----------------------------------------------------";

print "";


// ==========================================================

//  ERROR TEST: The lines below MUST cause errors

//  at COMPILE time.


//  Instructions: Uncomment ONE line at a time, save,

//  and try to compile with 'cmake --build build'.

//  You should see your compiler complaining!

// ==========================================================

print "--- Testing Type Errors at Compile Time ---";

// Uncomment the line below to test string to int assignment

// int error_assignment_1 = "not a number";

// Uncomment the line below to test number to string assignment

// string error_assignment_2 = 12345;

// Uncomment the line below to test invalid operation

// int error_operation = my_integer + my_text;

// Uncomment the line below to test another invalid operation

// string error_operation_2 = my_text - my_integer;

## How to Contribute ##
### We appreciate your interest in contributing to the Sapphire project! Currently, the primary way to contribute is by reporting issues. ###

## Reporting Issues ##
### If you find a bug, have a feature suggestion, or any other question, please open a new Issue in our repository. When opening an issue, please provide as much detail as possible, including: ###

### Clear description of the problem /suggestion. ###

### Steps to reproduce (if it's a bug). ###

### Expected behavior vs. observed behavior. ###

### Information about your environment (OS, compiler version, etc.). ###

## Author ##
### foxzyt ###

## License ##
### This project is licensed under the MIT License - see the LICENSE file for details. ###
