#include <iostream>
#include <string>
#include "quartz.h"

void display_help() {
    std::cout << "Quartz Benchmarking Suite - The official micro-benchmarking engine for Sapphire.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  quartz run <all | category | benchmark_name> [options]\n";
    std::cout << "  quartz list\n";
    std::cout << "  quartz compare <bench1> <bench2> [options]\n";
    std::cout << "  quartz --help, -h\n\n";
    std::cout << "Options:\n";
    std::cout << "  -d, --duration <ms>           Sets the micro-benchmark run duration in milliseconds (default: 1000ms).\n";
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

    quartz::QuartzConfig config;

    if (command == "list") {
        quartz::list_benchmarks();
        return 0;
    }

    if (command == "run") {
        if (argc < 3) {
            std::cerr << "Usage: quartz run <all | category | benchmark_name> [options]\n";
            return 1;
        }
        std::string target = argv[2];
        
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-d" || arg == "--duration") && i + 1 < argc) {
                try {
                    config.duration_ms = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "Invalid duration value: " << argv[i] << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Unknown option: " << arg << "\n";
                display_help();
                return 1;
            }
        }
        
        quartz::run_benchmarks(target, config);
        return 0;
    }

    if (command == "compare") {
        if (argc < 4) {
            std::cerr << "Usage: quartz compare <bench1> <bench2> [options]\n";
            return 1;
        }
        std::string b1 = argv[2];
        std::string b2 = argv[3];
        
        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-d" || arg == "--duration") && i + 1 < argc) {
                try {
                    config.duration_ms = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "Invalid duration value: " << argv[i] << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Unknown option: " << arg << "\n";
                display_help();
                return 1;
            }
        }
        
        quartz::compare_benchmarks(b1, b2, config);
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    display_help();
    return 1;
}
