#include <iostream>
#include <filesystem>
#include <string>
#include <algorithm>

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
#include "commands/uninstall.hpp"
#include "commands/update.hpp"
#include "commands/check.hpp"
#include "commands/list.hpp"
#include "commands/info.hpp"

#include "termcolor.hpp"

namespace mine {

void print_help() {
    std::cout << termcolor::cyan << "Mine Package Manager for Sapphire" << termcolor::reset << std::endl;
    std::cout << "Version: 2.1.0" << std::endl;
    std::cout << "Repository: https://github.com/foxzyt/sapphire-mine" << std::endl;
    std::cout << std::endl;
    
    std::cout << termcolor::bold << "Commands:" << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::green << "mine init" << termcolor::reset << "              - Create a new plugin structure interactively" << std::endl;
    std::cout << "  " << termcolor::green << "mine expand <version>" << termcolor::reset << "  - Add a new version to the plugin" << std::endl;
    std::cout << "  " << termcolor::green << "mine install <name>" << termcolor::reset << "    - Install a plugin from the registry" << std::endl;
    std::cout << "  " << termcolor::green << "mine install <name> <version>" << termcolor::reset << " - Install a specific version" << std::endl;
    std::cout << "  " << termcolor::green << "mine install <name> --local" << termcolor::reset << " - Install into project's plugins/ folder" << std::endl;
    std::cout << "  " << termcolor::red << "mine uninstall <name>" << termcolor::reset << "  - Remove a plugin (local and global)" << std::endl;
    std::cout << "  " << termcolor::red << "mine uninstall <name> --local" << termcolor::reset << " - Remove only local installation" << std::endl;
    std::cout << "  " << termcolor::yellow << "mine update" << termcolor::reset << "           - Check all plugins for updates" << std::endl;
    std::cout << "  " << termcolor::yellow << "mine update <name>" << termcolor::reset << "     - Check a specific plugin for updates" << std::endl;
    std::cout << "  " << termcolor::blue << "mine list" << termcolor::reset << "              - List all installed plugins" << std::endl;
    std::cout << "  " << termcolor::blue << "mine info <name>" << termcolor::reset << "       - Show detailed information about a plugin" << std::endl;
    std::cout << "  " << termcolor::cyan << "mine check" << termcolor::reset << "             - Run diagnostic on installed plugins" << std::endl;
    std::cout << "  " << termcolor::cyan << "mine help" << termcolor::reset << "              - Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << termcolor::yellow << "Note: When inside a Sapphire project (has main.sp or plugins/ folder)," << termcolor::reset << std::endl;
    std::cout << termcolor::yellow << "      plugins are installed locally in ./plugins/ by default." << termcolor::reset << std::endl;
    std::cout << termcolor::yellow << "      Use --global flag to install/uninstall globally." << termcolor::reset << std::endl;
}

} // namespace mine

int main(int argc, char* argv[]) {
    if (argc < 2) {
        mine::print_help();
        return 0;
    }
    
    std::string command = argv[1];
    std::filesystem::path working_dir = std::filesystem::current_path();
    
    // Check for --local/--global flags in remaining arguments
    bool local_flag = false;
    bool global_flag = false;
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--local") local_flag = true;
        if (arg == "--global") global_flag = true;
    }
    
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
                std::cerr << termcolor::red << "[!] Missing version argument" << termcolor::reset << std::endl;
                std::cerr << "Usage: mine expand <version>" << std::endl;
                return 1;
            }
            // Skip flags
            std::string expand_version;
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg != "--local" && arg != "--global") {
                    expand_version = arg;
                    break;
                }
            }
            if (expand_version.empty()) {
                std::cerr << termcolor::red << "[!] Missing version argument" << termcolor::reset << std::endl;
                std::cerr << "Usage: mine expand <version>" << std::endl;
                return 1;
            }
            return mine::commands::cmd_expand(expand_version, working_dir);
        }
        else if (command == "install") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: mine install <name> [version] [--local] [--global]" << std::endl;
                return 1;
            }
            
            // Parse arguments: mine install <name> [version] [--local/--global]
            std::string plugin_name;
            std::string version = "latest";
            bool version_found = false;
            
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--local" || arg == "--global") continue;
                if (plugin_name.empty()) {
                    plugin_name = arg;
                } else if (!version_found) {
                    // Check if this looks like a version (not a flag)
                    if (arg.find("--") != 0) {
                        version = arg;
                        version_found = true;
                    }
                }
            }
            
            if (plugin_name.empty()) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: mine install <name> [version] [--local] [--global]" << std::endl;
                return 1;
            }
            
            // If version contains "latest" or is a flag-like string, default to "latest"
            if (version == "--local" || version == "--global") {
                version = "latest";
            }
            
            // If we're in a Sapphire project and no --global flag, install locally
            bool install_local = false;
            if (mine::is_sapphire_project() && !global_flag) {
                install_local = true;
            }
            if (local_flag) install_local = true;
            if (global_flag) install_local = false;
            
            if (install_local) {
                std::cout << termcolor::yellow << "[*] Installing in local project scope (./plugins/)" << termcolor::reset << std::endl;
            }
            
            return mine::commands::cmd_install(plugin_name, version, install_local);
        }
        else if (command == "uninstall") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: mine uninstall <name> [--local] [--global]" << std::endl;
                return 1;
            }
            
            std::string uninstall_name;
            bool uninstall_local_only = false;
            
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--local") {
                    uninstall_local_only = true;
                } else if (arg == "--global") {
                    uninstall_local_only = false;
                } else if (uninstall_name.empty()) {
                    uninstall_name = arg;
                }
            }
            
            if (uninstall_name.empty()) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: mine uninstall <name> [--local] [--global]" << std::endl;
                return 1;
            }
            
            // Auto-detect: if in a project, uninstall local only by default
            if (mine::is_sapphire_project() && !uninstall_local_only && !global_flag) {
                std::cout << termcolor::yellow << "[*] Uninstalling from local project scope (./plugins/)" << termcolor::reset << std::endl;
                std::cout << termcolor::yellow << "[*] Use --global to uninstall from AppData" << termcolor::reset << std::endl;
                uninstall_local_only = true;
            }
            
            return mine::commands::cmd_uninstall(uninstall_name, uninstall_local_only);
        }
        else if (command == "update") {
            if (argc >= 3) {
                std::string update_name;
                for (int i = 2; i < argc; i++) {
                    std::string arg = argv[i];
                    if (arg != "--local" && arg != "--global") {
                        update_name = arg;
                        break;
                    }
                }
                return mine::commands::cmd_update(update_name);
            }
            return mine::commands::cmd_update();
        }
        else if (command == "check") {
            return mine::commands::cmd_check();
        }
        else if (command == "list") {
            return mine::commands::cmd_list();
        }
        else if (command == "info") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: mine info <name>" << std::endl;
                return 1;
            }
            std::string info_name;
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg != "--local" && arg != "--global") {
                    info_name = arg;
                    break;
                }
            }
            return mine::commands::cmd_info(info_name);
        }
        else {
            std::cerr << termcolor::red << "[!] Unknown command: " << command << termcolor::reset << std::endl;
            std::cerr << "Run 'mine help' for usage information" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << termcolor::red << "[!] Fatal error: " << e.what() << termcolor::reset << std::endl;
        return 1;
    }
    
    return 0;
}
