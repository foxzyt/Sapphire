#ifndef TOPAZ_COMMANDS_UNINSTALL_HPP
#define TOPAZ_COMMANDS_UNINSTALL_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/lockfile.hpp"
#include <iostream>
#include <filesystem>
#include "termcolor.hpp"

namespace topaz {
namespace commands {

// Check if a plugin is a dependency of any other installed plugin
inline bool is_plugin_required_by_others(const std::string& plugin_name) {
    fs::path plugin_dir = get_plugin_dir();
    if (!fs::exists(plugin_dir)) return false;
    
    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (!fs::is_directory(entry.path())) continue;
        std::string other_name = entry.path().filename().string();
        if (other_name == plugin_name || other_name == ".cache") continue;
        
        // Check all versions of this plugin for dependencies
        fs::path versions_dir = entry.path() / "versions";
        if (!fs::exists(versions_dir)) continue;
        
        for (const auto& version_entry : fs::directory_iterator(versions_dir)) {
            if (!fs::is_directory(version_entry.path())) continue;
            
            fs::path deps_path = version_entry.path() / "DEPENDENCIES.txt";
            if (fs::exists(deps_path)) {
                auto deps = parse_dependencies_txt(deps_path);
                for (const auto& dep : deps) {
                    if (dep.name == plugin_name) {
                        return true;
                    }
                }
            }
        }
    }
    
    // Also check the local project scope
    if (is_sapphire_project()) {
        fs::path local_dir = get_local_plugin_dir();
        if (fs::exists(local_dir)) {
            for (const auto& entry : fs::directory_iterator(local_dir)) {
                if (!fs::is_directory(entry.path())) continue;
                std::string other_name = entry.path().filename().string();
                if (other_name == plugin_name) continue;
                
                fs::path versions_dir = entry.path() / "versions";
                if (!fs::exists(versions_dir)) continue;
                
                for (const auto& version_entry : fs::directory_iterator(versions_dir)) {
                    if (!fs::is_directory(version_entry.path())) continue;
                    
                    fs::path deps_path = version_entry.path() / "DEPENDENCIES.txt";
                    if (fs::exists(deps_path)) {
                        auto deps = parse_dependencies_txt(deps_path);
                        for (const auto& dep : deps) {
                            if (dep.name == plugin_name) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return false;
}

// Uninstall a plugin - removes it from local or global scope
inline int cmd_uninstall(const std::string& plugin_name, bool local_only = false) {
    if (plugin_name.empty()) {
        std::cerr << termcolor::red << "[!] Plugin name cannot be empty" << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz uninstall <name> [--local] [--global]" << std::endl;
        return 1;
    }
    
    // Determine if we should uninstall locally or globally
    bool has_local = is_plugin_installed_local(plugin_name);
    bool has_global = is_plugin_installed(plugin_name);
    
    if (!has_local && !has_global) {
        std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' is not installed" << termcolor::reset << std::endl;
        if (is_sapphire_project()) {
            std::cerr << termcolor::yellow << "    Checked: local (./plugins/) and global (AppData)" << termcolor::reset << std::endl;
        }
        return 1;
    }
    
    // Check if plugin is required by others
    if (is_plugin_required_by_others(plugin_name)) {
        std::cerr << termcolor::yellow << "[!] Warning: Plugin '" << plugin_name << "' is required by other plugins" << termcolor::reset << std::endl;
        std::cerr << termcolor::yellow << "    Uninstalling it may break dependent plugins" << termcolor::reset << std::endl;
        std::cout << "Continue uninstall? (y/N): ";
        std::string response;
        std::getline(std::cin, response);
        response = trim(response);
        if (response != "y" && response != "Y") {
            if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Uninstall cancelled" << termcolor::reset << std::endl;
            return 0;
        }
    }
    
    if (local_only && !has_local && has_global) {
        std::cerr << termcolor::yellow << "[!] Plugin '" << plugin_name << "' is installed globally, but not locally in this project." << termcolor::reset << std::endl;
        std::cerr << termcolor::yellow << "    Use 'topaz uninstall " << plugin_name << " --global' to remove the global installation." << termcolor::reset << std::endl;
        return 1;
    }
    
    int removed_count = 0;
    
    // Remove local installation
    if (has_local && (!local_only || is_sapphire_project())) {
        fs::path local_plugin_path = get_local_plugin_dir() / plugin_name;
        if (topaz::g_verbose) std::cout << termcolor::cyan << "[*] Removing local plugin: " << local_plugin_path << termcolor::reset << std::endl;
        
        try {
            // Remove the lock file first
            fs::path lockfile = local_plugin_path / "topaz.lock";
            if (fs::exists(lockfile)) {
                fs::remove(lockfile);
                if (topaz::g_verbose) std::cout << termcolor::green << "[+] Removed lockfile" << termcolor::reset << std::endl;
            }
            
            // Remove CHECKSUMS.txt
            fs::path checksums = local_plugin_path / "CHECKSUMS.txt";
            if (fs::exists(checksums)) {
                fs::remove(checksums);
            }
            
            // Remove the entire plugin directory
            fs::remove_all(local_plugin_path);
            if (topaz::g_verbose) std::cout << termcolor::green << "[+] Removed plugin directory" << termcolor::reset << std::endl;
            removed_count++;
        } catch (const std::exception& e) {
            std::cerr << termcolor::red << "[!] Failed to remove local plugin: " << e.what() << termcolor::reset << std::endl;
        }
    }
    
    // Remove global installation
    if (has_global && !local_only) {
        fs::path global_plugin_path = get_plugin_dir() / plugin_name;
        if (topaz::g_verbose) std::cout << termcolor::cyan << "[*] Removing global plugin: " << global_plugin_path << termcolor::reset << std::endl;
        
        try {
            // Remove the lock file first
            fs::path lockfile = global_plugin_path / "topaz.lock";
            if (fs::exists(lockfile)) {
                fs::remove(lockfile);
                if (topaz::g_verbose) std::cout << termcolor::green << "[+] Removed lockfile" << termcolor::reset << std::endl;
            }
            
            // Remove CHECKSUMS.txt
            fs::path checksums = global_plugin_path / "CHECKSUMS.txt";
            if (fs::exists(checksums)) {
                fs::remove(checksums);
            }
            
            // Remove the entire plugin directory
            fs::remove_all(global_plugin_path);
            if (topaz::g_verbose) std::cout << termcolor::green << "[+] Removed plugin directory" << termcolor::reset << std::endl;
            removed_count++;
        } catch (const std::exception& e) {
            std::cerr << termcolor::red << "[!] Failed to remove global plugin: " << e.what() << termcolor::reset << std::endl;
        }
    }
    
    // Update the central lock registry if it exists
    if (removed_count > 0) {
        fs::path registry_lock = get_plugin_dir() / "topaz.lock";
        if (fs::exists(registry_lock)) {
            LockFile registry(registry_lock.parent_path(), "topaz", "registry");
            if (registry.read()) {
                // Remove the plugin from the registry lock
                registry.clear();
                // Re-write with remaining plugins
                // Actually, we just remove the entry from the lock file
                if (topaz::g_verbose) std::cout << termcolor::cyan << "[*] Updating central lock registry..." << termcolor::reset << std::endl;
                // For now, we just delete the lock file since we track via directories
                // fs::remove(registry_lock);
            }
        }
    }
    
    if (removed_count > 0) {
        std::cout << termcolor::green << "[OK] Uninstalled '" << plugin_name << "'" << termcolor::reset;
        if (removed_count > 1) {
            std::cout << " (local + global)";
        } else if (has_local) {
            std::cout << " (local)";
        } else {
            std::cout << " (global)";
        }
        std::cout << std::endl;
        return 0;
    } else {
        std::cerr << termcolor::red << "[!] Failed to uninstall '" << plugin_name << "'" << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_UNINSTALL_HPP
