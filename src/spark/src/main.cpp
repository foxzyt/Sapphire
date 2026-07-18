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
#include "core/semver.hpp"
#include "core/sapphire_version.hpp"

// Command headers
#include "commands/init.hpp"
#include "commands/expand.hpp"
#include "commands/install.hpp"
#include "commands/uninstall.hpp"
#include "commands/update.hpp"
#include "commands/check.hpp"
#include "commands/list.hpp"
#include "commands/info.hpp"
#include "commands/search.hpp"
#include "commands/sapphire_cmd.hpp"
#include "commands/upgrade.hpp"
#include "commands/outdated.hpp"
#include "commands/cache_clean.hpp"
#include "commands/cache_dir.hpp"
#include "commands/lock.hpp"
#include "commands/tree.hpp"
#include "commands/purge.hpp"

#include "termcolor.hpp"

namespace spark {

void print_help() {
    std::cout << termcolor::cyan << termcolor::bold
              << "Spark Package Manager for Sapphire"
              << termcolor::reset << std::endl;
    std::cout << "Version: 2.3.0" << std::endl;
    std::cout << "Repository: https://github.com/foxzyt/sapphire-spark" << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::bold << "Spark Self-Update:" << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::yellow << "spark upgrade"
              << termcolor::reset << "                 - Download and install the latest spark version" << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::bold << "Plugin Commands:" << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::green  << "spark init"
              << termcolor::reset << "                   - Create a new plugin structure interactively" << std::endl;
    std::cout << "  " << termcolor::green  << "spark expand <version>"
              << termcolor::reset << "         - Add a new version to the plugin" << std::endl;
    std::cout << "  " << termcolor::green  << "spark install <name>"
              << termcolor::reset << "           - Install a plugin from the registry" << std::endl;
    std::cout << "  " << termcolor::green  << "spark install <name> <version>"
              << termcolor::reset << "  - Install a specific version" << std::endl;
    std::cout << "  " << termcolor::green  << "spark install <name> --local"
              << termcolor::reset << "    - Install into project's plugins/ folder" << std::endl;
    std::cout << "  " << termcolor::red    << "spark uninstall <name>"
              << termcolor::reset << "         - Remove a plugin (local and global)" << std::endl;
    std::cout << "  " << termcolor::red    << "spark uninstall <name> --local"
              << termcolor::reset << "  - Remove only local installation" << std::endl;
    std::cout << "  " << termcolor::yellow << "spark update"
              << termcolor::reset << "                   - Check all plugins for updates" << std::endl;
    std::cout << "  " << termcolor::yellow << "spark update <name>"
              << termcolor::reset << "            - Check a specific plugin for updates" << std::endl;
    std::cout << "  " << termcolor::blue   << "spark list"
              << termcolor::reset << "                   - List installed plugins + runtime info" << std::endl;
    std::cout << "  " << termcolor::blue   << "spark info <name>"
              << termcolor::reset << "              - Show detailed information about a plugin" << std::endl;
    std::cout << "  " << termcolor::magenta << "spark search <query>"
              << termcolor::reset << "         - Search for plugins in the registry" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark check"
              << termcolor::reset << "                  - Run diagnostic on installed plugins" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark outdated"
              << termcolor::reset << "               - List plugins with newer versions available" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark outdated <name>"
              << termcolor::reset << "         - Check a specific plugin for new versions" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark lock <name> [version]"
              << termcolor::reset << "       - Generate lock file for a plugin" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark tree"
              << termcolor::reset << "                   - Show dependency tree for all plugins" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark tree <name> [version]"
              << termcolor::reset << "    - Show dependency tree for a specific plugin" << std::endl;
    std::cout << "  " << termcolor::red    << "spark purge <name> [version]"
              << termcolor::reset << "  - Remove specific version(s) of a plugin" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark help"
              << termcolor::reset << "                   - Show this help message" << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::bold << "Cache Commands:" << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::yellow << "spark cache dir"
              << termcolor::reset << "              - Show cache directory path and contents" << std::endl;
    std::cout << "  " << termcolor::red   << "spark cache clean"
              << termcolor::reset << "            - Clean the download cache" << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::bold << termcolor::magenta
              << "Sapphire Runtime Commands:"
              << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::green  << "spark sapphire list"
              << termcolor::reset << "            - List available Sapphire versions (remote)" << std::endl;
    std::cout << "  " << termcolor::green  << "spark sapphire install <version>"
              << termcolor::reset << "  - Install and activate a Sapphire release" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark sapphire use <version>"
              << termcolor::reset << "       - Switch to an installed version" << std::endl;
    std::cout << "  " << termcolor::cyan   << "spark sapphire current"
              << termcolor::reset << "         - Show the active Sapphire version" << std::endl;
    std::cout << "  " << termcolor::blue   << "spark sapphire versions"
              << termcolor::reset << "        - List locally installed versions" << std::endl;
    std::cout << "  " << termcolor::red    << "spark sapphire uninstall <version>"
              << termcolor::reset << " - Remove an installed version" << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::magenta << "Version Formatting (SemVer):" << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::cyan << "latest"    << termcolor::reset
              << "                   - Resolves to the highest available version" << std::endl;
    std::cout << "  " << termcolor::cyan << "1.0.0"     << termcolor::reset
              << "                    - Exact version match" << std::endl;
    std::cout << "  " << termcolor::cyan << "\"^1.0.0\"" << termcolor::reset
              << "                 - Compatible updates (same major version)" << std::endl;
    std::cout << "  " << termcolor::cyan << "\">1.0.0\"" << termcolor::reset
              << " , " << termcolor::cyan << "\"<2.0.0\""
              << termcolor::reset << "      - Greater than / Less than a specific version" << std::endl;
    std::cout << "  " << termcolor::cyan << "\">=1.0.0\""<< termcolor::reset
              << ", " << termcolor::cyan << "\"<=2.0.0\""
              << termcolor::reset << "      - Greater or equal / Less or equal" << std::endl;
    std::cout << termcolor::red
              << "  * Always use quotes (\"\") for >, <, ^ to avoid terminal redirection!"
              << termcolor::reset << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::yellow
              << "Note: When inside a Sapphire project (has main.sp or plugins/ folder),"
              << termcolor::reset << std::endl;
    std::cout << termcolor::yellow
              << "      plugins are installed locally in ./plugins/ by default."
              << termcolor::reset << std::endl;
    std::cout << termcolor::yellow
              << "      Use --global flag to install/uninstall globally."
              << termcolor::reset << std::endl;
}

} // namespace spark

int main(int argc, char* argv[]) {
    if (argc < 2) {
        spark::print_help();
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
            spark::print_help();
            return 0;
        }
        // -----------------------------------------------------------------
        // "spark sapphire <subcommand>" — Runtime version manager
        // argv[1] = "sapphire", argv[2] = subcommand, argv[3...] = args
        // -----------------------------------------------------------------
        else if (command == "sapphire") {
            return spark::commands::cmd_sapphire_dispatch(argc, argv, 2);
        }
        else if (command == "init") {
            return spark::commands::cmd_init(working_dir);
        }
        else if (command == "expand") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing version argument" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark expand <version>" << std::endl;
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
                std::cerr << "Usage: spark expand <version>" << std::endl;
                return 1;
            }
            return spark::commands::cmd_expand(expand_version, working_dir);
        }
        else if (command == "install") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark install <name> [version] [--local] [--global]" << std::endl;
                return 1;
            }
            
            // Parse arguments: spark install <name> [version] [--local/--global]
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
                std::cerr << "Usage: spark install <name> [version] [--local] [--global]" << std::endl;
                return 1;
            }
            
            // If version contains "latest" or is a flag-like string, default to "latest"
            if (version == "--local" || version == "--global") {
                version = "latest";
            }
            
            // If we're in a Sapphire project and no --global flag, install locally
            bool install_local = false;
            if (spark::is_sapphire_project() && !global_flag) {
                install_local = true;
            }
            if (local_flag) install_local = true;
            if (global_flag) install_local = false;
            
            if (install_local) {
                std::cout << termcolor::yellow << "[*] Installing in local project scope (./plugins/)" << termcolor::reset << std::endl;
            }
            
            return spark::commands::cmd_install(plugin_name, version, install_local);
        }
        else if (command == "uninstall") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark uninstall <name> [--local] [--global]" << std::endl;
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
                std::cerr << "Usage: spark uninstall <name> [--local] [--global]" << std::endl;
                return 1;
            }
            
            // Auto-detect: if in a project, uninstall local only by default
            if (spark::is_sapphire_project() && !uninstall_local_only && !global_flag) {
                std::cout << termcolor::yellow << "[*] Uninstalling from local project scope (./plugins/)" << termcolor::reset << std::endl;
                std::cout << termcolor::yellow << "[*] Use --global to uninstall from AppData" << termcolor::reset << std::endl;
                uninstall_local_only = true;
            }
            
            return spark::commands::cmd_uninstall(uninstall_name, uninstall_local_only);
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
                return spark::commands::cmd_update(update_name);
            }
            return spark::commands::cmd_update();
        }
        else if (command == "check") {
            return spark::commands::cmd_check();
        }
        else if (command == "list") {
            return spark::commands::cmd_list();
        }
        else if (command == "upgrade") {
            return spark::commands::cmd_upgrade();
        }
        else if (command == "info") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark info <name>" << std::endl;
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
            return spark::commands::cmd_info(info_name);
        }
        else if (command == "search") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing search query" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark search <query>" << std::endl;
                return 1;
            }
            return spark::commands::cmd_search(argv[2]);
        }
        // -----------------------------------------------------------------
        // spark outdated [name] — Check for newer versions
        // -----------------------------------------------------------------
        else if (command == "outdated") {
            if (argc >= 3) {
                std::string outdated_name;
                for (int i = 2; i < argc; i++) {
                    std::string arg = argv[i];
                    if (arg != "--local" && arg != "--global") {
                        outdated_name = arg;
                        break;
                    }
                }
                return spark::commands::cmd_outdated(outdated_name);
            }
            return spark::commands::cmd_outdated();
        }
        // -----------------------------------------------------------------
        // spark cache <subcommand> — Cache management
        // -----------------------------------------------------------------
        else if (command == "cache") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing cache subcommand" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark cache clean | spark cache dir" << std::endl;
                return 1;
            }
            std::string cache_subcmd = argv[2];
            if (cache_subcmd == "clean") {
                return spark::commands::cmd_cache_clean();
            } else if (cache_subcmd == "dir") {
                return spark::commands::cmd_cache_dir();
            } else {
                std::cerr << termcolor::red << "[!] Unknown cache subcommand: '" << cache_subcmd << "'" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark cache clean | spark cache dir" << std::endl;
                return 1;
            }
        }
        // -----------------------------------------------------------------
        // spark lock <name> [version] — Generate lock file
        // -----------------------------------------------------------------
        else if (command == "lock") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark lock <name> [version]" << std::endl;
                return 1;
            }
            std::string lock_name;
            std::string lock_version = "latest";
            bool lock_version_found = false;
            
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--local" || arg == "--global") continue;
                if (lock_name.empty()) {
                    lock_name = arg;
                } else if (!lock_version_found && arg.find("--") != 0) {
                    lock_version = arg;
                    lock_version_found = true;
                }
            }
            
            if (lock_name.empty()) {
                std::cerr << termcolor::red << "[!] Missing plugin name" << termcolor::reset << std::endl;
                std::cerr << "Usage: spark lock <name> [version]" << std::endl;
                return 1;
            }
            
            return spark::commands::cmd_lock_generate(lock_name, lock_version);
        }
        // -----------------------------------------------------------------
        // spark tree [name] [version] — Show dependency tree
        // -----------------------------------------------------------------
        else if (command == "tree") {
            if (argc >= 3) {
                std::string tree_name;
                std::string tree_version = "latest";
                bool tree_version_found = false;
                
                for (int i = 2; i < argc; i++) {
                    std::string arg = argv[i];
                    if (arg == "--local" || arg == "--global") continue;
                    if (tree_name.empty()) {
                        tree_name = arg;
                    } else if (!tree_version_found && arg.find("--") != 0) {
                        tree_version = arg;
                        tree_version_found = true;
                    }
                }
                
                return spark::commands::cmd_tree(tree_name, tree_version);
            }
            return spark::commands::cmd_tree();
        }
        // -----------------------------------------------------------------
        // spark purge <name> [version] [--local] [--global] — Remove plugin versions
        // -----------------------------------------------------------------
        else if (command == "purge") {
            if (argc < 3) {
                std::cerr << termcolor::red << "[!] Missing plugin name." << termcolor::reset << std::endl;
                std::cerr << "Usage: spark purge <name> [version] [--local] [--global]" << std::endl;
                return 1;
            }

            std::string purge_name;
            std::string purge_version;
            bool purge_local_only = false;
            bool purge_global_only = false;
            bool purge_version_found = false;

            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--local") {
                    purge_local_only = true;
                } else if (arg == "--global") {
                    purge_global_only = true;
                } else if (purge_name.empty()) {
                    purge_name = arg;
                } else if (!purge_version_found && arg.find("--") != 0) {
                    purge_version = arg;
                    purge_version_found = true;
                }
            }

            if (purge_name.empty()) {
                std::cerr << termcolor::red << "[!] Missing plugin name." << termcolor::reset << std::endl;
                std::cerr << "Usage: spark purge <name> [version] [--local] [--global]" << std::endl;
                return 1;
            }

            return spark::commands::cmd_purge(purge_name, purge_version, purge_local_only, purge_global_only);
        }
        // -----------------------------------------------------------------
        else {
            std::cerr << termcolor::red << "[!] Unknown command: " << command << termcolor::reset << std::endl;
            std::cerr << "Run 'spark help' for usage information" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << termcolor::red << "[!] Fatal error: " << e.what() << termcolor::reset << std::endl;
        return 1;
    }
    
    return 0;
}