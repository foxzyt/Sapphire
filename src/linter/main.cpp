#include <iostream>
#include <string>
#include <algorithm>
#include "citrine.h"

void display_help() {
    std::cout << "Citrine Linter - The official static analysis tool for Sapphire.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  citrine lint <file.sp> [options]      Runs the linter and lists errors.\n";
    std::cout << "  citrine explain <file.sp> [options]   Lists errors with detailed explanations.\n";
    std::cout << "  citrine fix <file.sp>                 Interactive mode to auto-correct issues.\n";
    std::cout << "  citrine undo <file.sp>                Restores the file to its state before the last fix.\n";
    std::cout << "  citrine --help, -h                    Shows this help message.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --category, -c <name>                 Filter by category (security, performance, style, syntax, architecture).\n";
    std::cout << "  --level, -l <level>                   Filter by minimum warning level (info, pedantic, warning, error).\n";
}

bool parse_filters(int argc, char* argv[], citrine::FilterConfig& config) {
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--category" || arg == "-c") && i + 1 < argc) {
            std::string cat = argv[++i];
            std::transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
            config.has_category_filter = true;
            if (cat == "syntax") config.category_filter = citrine::Category::SYNTAX;
            else if (cat == "style") config.category_filter = citrine::Category::STYLE;
            else if (cat == "performance") config.category_filter = citrine::Category::PERFORMANCE;
            else if (cat == "security") config.category_filter = citrine::Category::SECURITY;
            else if (cat == "architecture") config.category_filter = citrine::Category::ARCHITECTURE;
            else {
                std::cerr << "Error: Unknown category '" << cat << "'\n";
                return false;
            }
        } else if ((arg == "--level" || arg == "-l") && i + 1 < argc) {
            std::string lvl = argv[++i];
            std::transform(lvl.begin(), lvl.end(), lvl.begin(), ::tolower);
            config.has_level_filter = true;
            if (lvl == "info") config.level_filter = citrine::WarningLevel::INFO;
            else if (lvl == "pedantic") config.level_filter = citrine::WarningLevel::PEDANTIC;
            else if (lvl == "warning") config.level_filter = citrine::WarningLevel::WARNING;
            else if (lvl == "error" || lvl == "err") config.level_filter = citrine::WarningLevel::ERR;
            else {
                std::cerr << "Error: Unknown warning level '" << lvl << "'\n";
                return false;
            }
        }
    }
    return true;
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

    citrine::FilterConfig config;

    if (command == "lint") {
        if (argc >= 3) {
            if (!parse_filters(argc, argv, config)) return 1;
            citrine::run_lint(argv[2], citrine::MODE_LINT, config);
            return 0;
        } else {
            std::cerr << "Usage: citrine lint <file.sp> [options]\n";
            return 1;
        }
    } else if (command == "explain") {
        if (argc >= 3) {
            if (!parse_filters(argc, argv, config)) return 1;
            citrine::run_lint(argv[2], citrine::MODE_EXPLAIN, config);
            return 0;
        } else {
            std::cerr << "Usage: citrine explain <file.sp> [options]\n";
            return 1;
        }
    } else if (command == "fix") {
        if (argc >= 3) {
            citrine::run_lint(argv[2], citrine::MODE_FIX, config);
            return 0;
        } else {
            std::cerr << "Usage: citrine fix <file.sp>\n";
            return 1;
        }
    } else if (command == "undo") {
        if (argc >= 3) {
            citrine::run_lint(argv[2], citrine::MODE_UNDO, config);
            return 0;
        } else {
            std::cerr << "Usage: citrine undo <file.sp>\n";
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << "\n";
    display_help();
    return 1;
}
