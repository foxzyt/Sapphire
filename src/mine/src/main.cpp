#include <iostream>
#include <filesystem>
#include <string>

// Core headers
#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include "core/resolver.hpp"

// Command headers
#include "commands/init.hpp"
#include "commands/expand.hpp"
#include "commands/install.hpp"
#include "commands/check.hpp"
#include "commands/list.hpp"
#include "commands/info.hpp"

#include "termcolor.hpp"

namespace mine {

void print_help() {
    std::cout << "Mine Package Manager for Sapphire" << std::endl;
    std::cout << "Version: 2.0.0" << std::endl;
    std::cout << "Repository: https://github.com/foxzyt/sapphire-mine" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Commands:" << std::endl;
    std::cout << "  mine init              - Create a new plugin structure interactively" << std::endl;
    std::cout << "  mine expand <version>  - Add a new version to the plugin" << std::endl;
    std::cout << "  mine install <name>    - Install a plugin from the registry" << std::endl;
    std::cout << "  mine check             - Run diagnostic on installed plugins" << std::endl;
    std::cout << "  mine list              - List all installed plugins" << std::endl;
    std::cout << "  mine info <name>       - Show detailed information about a plugin" << std::endl;
    std::cout << "  mine help              - Show this help message" << std::endl;
}

} // namespace mine

int main(int argc, char* argv[]) {
    if (argc < 2) {
        mine::print_help();
        return 0;
    }
    
    std::string command = argv[1];
    std::filesystem::path working_dir = std::filesystem::current_path();
    
    try {
        if (command == "help" || command == "--help" || command == "-h") {
            mine::print_help();
            return 0;
        }
        else if (command == "init") {
            return mine::commands::cmd_init(working_dir);
        }
        else if (command == "expand") {
            if (argc < 3) {
                std::cerr << "[!] Missing version argument" << std::endl;
                std::cerr << "Usage: mine expand <version>" << std::endl;
                return 1;
            }
            return mine::commands::cmd_expand(argv[2], working_dir);
        }
        else if (command == "install") {
            if (argc < 3) {
                std::cerr << "[!] Missing plugin name" << std::endl;
                std::cerr << "Usage: mine install <name> [version]" << std::endl;
                return 1;
            }
            std::string version = (argc >= 4) ? argv[3] : "latest";
            return mine::commands::cmd_install(argv[2], version);
        }
        else if (command == "check") {
            return mine::commands::cmd_check();
        }
        else if (command == "list") {
            return mine::commands::cmd_list();
        }
        else if (command == "info") {
            if (argc < 3) {
                std::cerr << "[!] Missing plugin name" << std::endl;
                std::cerr << "Usage: mine info <name>" << std::endl;
                return 1;
            }
            return mine::commands::cmd_info(argv[2]);
        }
        else {
            std::cerr << "[!] Unknown command: " << command << std::endl;
            std::cerr << "Run 'mine help' for usage information" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[!] Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
