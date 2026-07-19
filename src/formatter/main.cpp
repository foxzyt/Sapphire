#include <iostream>
#include <string>
#include "amethyst.h"

void display_help() {
    std::cout << "Amethyst Formatter - The official automatic code formatter for Sapphire.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  amethyst format <file.sp>     Formats the specified file in-place.\n";
    std::cout << "  amethyst check <file.sp>      Checks if the file complies with standard formatting.\n";
    std::cout << "  amethyst --help, -h           Shows this help message.\n";
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

    if (command == "format") {
        if (argc >= 3) {
            bool success = amethyst::format_file(argv[2], false);
            return success ? 0 : 1;
        } else {
            std::cerr << "Usage: amethyst format <file.sp>\n";
            return 1;
        }
    } else if (command == "check") {
        if (argc >= 3) {
            bool matches = amethyst::format_file(argv[2], true);
            if (matches) {
                std::cout << "✨ \x1b[32mFormatting check passed:\x1b[0m " << argv[2] << "\n";
                return 0;
            } else {
                std::cout << "❌ \x1b[31mFormatting check failed (file needs formatting):\x1b[0m " << argv[2] << "\n";
                return 1;
            }
        } else {
            std::cerr << "Usage: amethyst check <file.sp>\n";
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << "\n";
    display_help();
    return 1;
}
