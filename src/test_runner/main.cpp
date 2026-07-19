#include <iostream>
#include <string>
#include "garnet.h"

void display_help() {
    std::cout << "Garnet Test Runner - The official test execution tool for Sapphire.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  garnet run [file.sp|dir] [options]    Runs all tests in the specified file or directory.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -f, --filter <pattern>                Only run tests matching the substring pattern.\n";
    std::cout << "  -r, --retries <count>                 Rerun failed tests up to <count> times.\n";
    std::cout << "  -o, --output <file.json>              Export the complete test results report to a JSON file.\n";
    std::cout << "  -h, --help                            Shows this help message.\n";
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
        std::string target_path = ".";
        garnet::GarnetConfig config;
        
        int arg_idx = 2;
        if (argc >= 3 && argv[2][0] != '-') {
            target_path = argv[2];
            arg_idx = 3;
        }
        
        for (int i = arg_idx; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-f" || arg == "--filter") && i + 1 < argc) {
                config.filter = argv[++i];
            } else if ((arg == "-r" || arg == "--retries") && i + 1 < argc) {
                try {
                    config.retries = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "Invalid retries count: " << argv[i] << "\n";
                    return 1;
                }
            } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                config.output_path = argv[++i];
            } else {
                std::cerr << "Unknown or invalid option: " << arg << "\n";
                display_help();
                return 1;
            }
        }
        
        garnet::run_tests(target_path, config);
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    display_help();
    return 1;
}
