#ifndef MINE_COMMANDS_INSTALL_HPP
#define MINE_COMMANDS_INSTALL_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include "core/resolver.hpp"
#include <iostream>
#include <filesystem>
#include "termcolor.hpp"

namespace mine {
namespace commands {

// Install command with dependency resolution
inline int cmd_install(const std::string& plugin_name, const std::string& version = "latest") {
    if (plugin_name.empty()) {
        std::cerr << termcolor::red << "[!] Plugin name cannot be empty" << termcolor::reset << std::endl;
        std::cerr << "Usage: mine install <name> [version]" << std::endl;
        return 1;
    }
    
    std::cout << termcolor::cyan << "[*] Installing plugin: " << plugin_name << termcolor::reset;
    if (version != "latest") {
        std::cout << " (version: " << version << ")";
    }
    std::cout << std::endl;
    
    // Query the registry first
    auto registry_entry = query_registry(plugin_name);
    if (!registry_entry) {
        std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' not found in central registry" << termcolor::reset << std::endl;
        return 1;
    }
    
    std::cout << termcolor::blue << "[*] Repository: " << registry_entry->repository << termcolor::reset << std::endl;
    
    // Check if already installed
    if (is_plugin_installed(plugin_name) && version == "latest") {
        fs::path plugin_path = get_plugin_dir() / plugin_name / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_path);
        if (meta) {
            std::cout << termcolor::yellow << "[*] Plugin already installed (v" << meta->version << ")" << termcolor::reset << std::endl;
            
            // Check for warnings
            if (meta->deprecated) {
                std::cout << termcolor::red << "[!] WARNING: This plugin is DEPRECATED" << termcolor::reset << std::endl;
            }
            if (!meta->notice.empty()) {
                std::cout << termcolor::yellow << "[!] NOTICE: " << meta->notice << termcolor::reset << std::endl;
            }
            
            std::cout << "Continue installation? (Y/n): ";
            std::string response;
            std::getline(std::cin, response);
            response = trim(response);
            
            if (response == "n" || response == "N") {
                std::cout << termcolor::yellow << "[*] Installation cancelled" << termcolor::reset << std::endl;
                return 0;
            }
        }
    }
    
    // Perform installation with dependency resolution
    DependencyResolver resolver(fs::current_path(), plugin_name);
    resolver.resolve_and_install(plugin_name, version);
    
    // Write lock file
    resolver.write_lockfile();
    
    // Check if the requested plugin/version is now available
    bool success = false;
    if (version == "latest") {
        // For latest, just check if plugin is installed
        success = is_plugin_installed(plugin_name);
    } else {
        // For specific version, check if that version exists
        std::string clean_version = version;
        if (clean_version.size() > 0 && clean_version[0] == 'v') {
            clean_version = clean_version.substr(1);
        }
        fs::path version_dir = get_plugin_dir() / plugin_name / "versions" / ("v" + clean_version);
        success = fs::exists(version_dir);
    }
    
    if (success) {
        std::cout << termcolor::green << "[OK] Installed '" << plugin_name << "'" << termcolor::reset;
        if (resolver.get_total_installed() > 0) {
            std::cout << " and " << resolver.get_total_installed() << " dependencies";
        }
        std::cout << std::endl;
        return 0;
    } else {
        std::cerr << termcolor::red << "[!] Installation failed" << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace mine

#endif // MINE_COMMANDS_INSTALL_HPP
