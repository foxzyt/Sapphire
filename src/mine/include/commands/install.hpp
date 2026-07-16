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

// Install command with dependency resolution and local/global scope support
inline int cmd_install(const std::string& plugin_name, const std::string& version = "latest", bool local_scope = false) {
    if (plugin_name.empty()) {
        std::cerr << termcolor::red << "[!] Plugin name cannot be empty" << termcolor::reset << std::endl;
        std::cerr << "Usage: mine install <name> [version] [--local] [--global]" << std::endl;
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
    
    // Determine the target directory for installation
    fs::path target_plugin_dir;
    if (local_scope) {
        target_plugin_dir = get_local_plugin_dir() / plugin_name;
        std::cout << termcolor::yellow << "[*] Target: local project scope (./plugins/)" << termcolor::reset << std::endl;
    } else {
        target_plugin_dir = get_plugin_dir() / plugin_name;
        std::cout << termcolor::yellow << "[*] Target: global scope (AppData)" << termcolor::reset << std::endl;
    }
    
    // Check if already installed in the target scope
    if (local_scope ? is_plugin_installed_local(plugin_name) : is_plugin_installed(plugin_name)) {
        fs::path plugin_path = (local_scope ? get_local_plugin_dir() : get_plugin_dir()) / plugin_name / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_path);
        if (meta) {
            std::cout << termcolor::yellow << "[*] Plugin already installed in " 
                      << (local_scope ? "local" : "global") << " scope (v" << meta->version << ")" << termcolor::reset << std::endl;
            
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
    // The resolver downloads to global cache, but we need to move to local scope if requested
    DependencyResolver resolver(fs::current_path(), plugin_name);
    resolver.resolve_and_install(plugin_name, version);
    
    // Write lock files for all versions
    resolver.write_lockfiles();
    
    // If local scope, copy the plugin from global to local
    if (local_scope) {
        fs::path global_plugin = get_plugin_dir() / plugin_name;
        if (fs::exists(global_plugin)) {
            std::cout << termcolor::cyan << "[*] Copying to local project scope..." << termcolor::reset << std::endl;
            
            // Remove local copy if it exists
            if (fs::exists(target_plugin_dir)) {
                fs::remove_all(target_plugin_dir);
            }
            fs::create_directories(target_plugin_dir.parent_path());
            
            // Copy the entire plugin directory
            try {
                fs::copy(global_plugin, target_plugin_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                std::cout << termcolor::green << "[+] Copied to local project scope" << termcolor::reset << std::endl;
            } catch (const std::exception& e) {
                std::cerr << termcolor::red << "[!] Failed to copy to local scope: " << e.what() << termcolor::reset << std::endl;
                // Still consider it a success since it's installed globally
            }
        }
    }
    
    // Check if the requested plugin/version is now available
    bool success = false;
    if (version == "latest") {
        // For latest, just check if plugin is installed anywhere
        success = is_plugin_installed(plugin_name) || is_plugin_installed_local(plugin_name);
    } else {
        // For specific version, check if that version exists
        std::string clean_version = version;
        if (clean_version.size() > 0 && clean_version[0] == 'v') {
            clean_version = clean_version.substr(1);
        }
        fs::path version_dir = get_plugin_version_path(plugin_name, clean_version);
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
