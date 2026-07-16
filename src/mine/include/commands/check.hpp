#ifndef MINE_COMMANDS_CHECK_HPP
#define MINE_COMMANDS_CHECK_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include <iostream>
#include <filesystem>
#include "termcolor.hpp"

namespace mine {
namespace commands {

// Diagnostic command - check plugin integrity and dependencies
inline int cmd_check() {
    fs::path plugin_dir = get_plugin_dir();
    
    if (!fs::exists(plugin_dir)) {
        std::cout << "[*] No plugins directory found" << std::endl;
        return 0;
    }
    
    std::cout << "=== Plugin Diagnostic Report ===" << std::endl;
    std::cout << std::endl;
    
    int total_plugins = 0;
    int errors = 0;
    int warnings = 0;
    
    // Iterate through all plugin directories
    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (!fs::is_directory(entry.path())) {
            continue;
        }
        
        std::string plugin_name = entry.path().filename().string();
        
        // Skip cache directory
        if (plugin_name == ".cache") {
            continue;
        }
        
        total_plugins++;
        
        std::cout << "Plugin: " << plugin_name << std::endl;
        
        // Check for PLUGIN.txt
        fs::path plugin_txt = entry.path() / "PLUGIN.txt";
        if (!fs::exists(plugin_txt)) {
            std::cout << termcolor::red << "  [!] ERROR: Missing PLUGIN.txt" << termcolor::reset << std::endl;
            errors++;
            continue;
        }
        
        // Parse PLUGIN.txt
        auto meta = parse_plugin_txt(plugin_txt);
        if (!meta) {
            std::cout << termcolor::red << "  [!] ERROR: Failed to parse PLUGIN.txt" << termcolor::reset << std::endl;
            errors++;
            continue;
        }
        
        std::cout << "  Version: " << meta->version << std::endl;
        std::cout << "  Author: " << meta->author << std::endl;
        
        // Check for deprecation
        if (meta->deprecated) {
            std::cout << termcolor::yellow << "  [!] WARNING: Plugin is deprecated" << termcolor::reset << std::endl;
            warnings++;
        }
        
        // Check for notices
        if (!meta->notice.empty()) {
            std::cout << termcolor::cyan << "  [i] NOTICE: " << meta->notice << termcolor::reset << std::endl;
        }
        
        // Check versions directory
        fs::path versions_dir = entry.path() / "versions";
        if (!fs::exists(versions_dir)) {
            std::cout << termcolor::red << "  [!] ERROR: Missing versions directory" << termcolor::reset << std::endl;
            errors++;
            continue;
        }
        
        // Check for at least one version
        int version_count = 0;
        for (const auto& version_entry : fs::directory_iterator(versions_dir)) {
            if (fs::is_directory(version_entry.path())) {
                version_count++;
            }
        }
        
        if (version_count == 0) {
            std::cout << termcolor::red << "  [!] ERROR: No versions found" << termcolor::reset << std::endl;
            errors++;
        } else {
            std::cout << "  Versions: " << version_count << std::endl;
        }
        
        // Check dependencies for the latest version
        if (version_count > 0) {
            fs::path latest_version_path = versions_dir / ("v" + meta->version);
            if (fs::exists(latest_version_path)) {
                fs::path deps_txt = latest_version_path / "DEPENDENCIES.txt";
                
                if (fs::exists(deps_txt)) {
                    auto dependencies = parse_dependencies_txt(deps_txt);
                    
                    if (!dependencies.empty()) {
                        std::cout << "  Dependencies: " << dependencies.size() << std::endl;
                        
                        // Check if dependencies are installed
                        for (const auto& dep : dependencies) {
                            if (is_plugin_installed(dep.name)) {
                                std::cout << termcolor::green << "    [OK] " << dep.name << termcolor::reset << " (required: " << dep.version << ")" << std::endl;
                            } else {
                                std::cout << termcolor::red << "    [!] MISSING: " << dep.name << termcolor::reset << " (required: " << dep.version << ")" << std::endl;
                                errors++;
                            }
                        }
                    }
                }
            }
        }
        
        std::cout << std::endl;
    }
    
    // Summary
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Total plugins: " << total_plugins << std::endl;
    std::cout << "Errors: " << errors << std::endl;
    std::cout << "Warnings: " << warnings << std::endl;
    
    if (errors == 0 && warnings == 0) {
        std::cout << termcolor::green << "[OK] All plugins are healthy" << termcolor::reset << std::endl;
        return 0;
    } else if (errors == 0) {
        std::cout << termcolor::green << "[OK] No critical errors found" << termcolor::reset << std::endl;
        return 0;
    } else {
        std::cout << termcolor::red << "[!] Issues found - review above" << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace mine

#endif // MINE_COMMANDS_CHECK_HPP
