#include <iostream>
#include <string>
#include "garnet.h"

void display_help() {
    std::cout << "Garnet Test Runner - The official test execution tool for Sapphire.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  garnet run <file.sp|dir>      Runs all tests in the specified file or directory.\n";
    std::cout << "  garnet --help, -h             Shows this help message.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        display_help();
        return 1;
    }

    std::string command = argv[1];

    if (command == "-h" || command == "--help") {
        display_help();
        return 0;
    }

    if (command == "run") {
        if (argc >= 3) {
            garnet::run_tests(argv[2]);
            return 0;
        } else {
            // Run tests in current directory by default if no path is given
            garnet::run_tests(".");
            return 0;
        }
    }

    std::cerr << "Unknown command: " << command << "\n";
    display_help();
    return 1;
}
