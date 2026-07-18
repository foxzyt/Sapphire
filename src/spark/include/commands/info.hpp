#ifndef SPARK_COMMANDS_INFO_HPP
#define SPARK_COMMANDS_INFO_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include <iostream>
#include <filesystem>
#include "termcolor.hpp"

namespace spark {
namespace commands {

// Show detailed information about a plugin (local and global scope)
inline int cmd_info(const std::string& plugin_name) {
    if (plugin_name.empty()) {
        std::cerr << termcolor::red << "[!] Plugin name cannot be empty" << termcolor::reset << std::endl;
        std::cerr << "Usage: spark info <name>" << std::endl;
        return 1;
    }
    
    std::cout << termcolor::bold << "=== Plugin Information: " << plugin_name << " ===" << termcolor::reset << std::endl;
    std::cout << std::endl;
    
    // Check if plugin is installed globally
    bool has_global = is_plugin_installed(plugin_name);
    bool has_local = is_plugin_installed_local(plugin_name);
    
    // Show global installation info
    if (has_global) {
        std::cout << termcolor::bold << "Global Installation (AppData):" << termcolor::reset << std::endl;
        fs::path plugin_path = get_plugin_dir() / plugin_name / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_path);
        
        if (meta) {
            std::cout << "  Name: " << meta->name << std::endl;
            std::cout << "  Version: " << meta->version << std::endl;
            std::cout << "  Author: " << meta->author << std::endl;
            
            if (!meta->description.empty()) {
                std::cout << "  Description: " << meta->description << std::endl;
            }
            
            if (!meta->repository.empty()) {
                std::cout << "  Repository: " << meta->repository << std::endl;
            }
            
            if (meta->deprecated) {
                std::cout << termcolor::red << "  Status: DEPRECATED" << termcolor::reset << std::endl;
            }
            
            if (!meta->notice.empty()) {
                std::cout << termcolor::yellow << "  Notice: " << meta->notice << termcolor::reset << std::endl;
            }
            
            // Count versions
            fs::path versions_dir = get_plugin_dir() / plugin_name / "versions";
            if (fs::exists(versions_dir)) {
                int version_count = 0;
                std::cout << "  Versions: ";
                for (const auto& entry : fs::directory_iterator(versions_dir)) {
                    if (fs::is_directory(entry.path())) {
                        if (version_count > 0) std::cout << ", ";
                        std::cout << entry.path().filename().string();
                        version_count++;
                    }
                }
                std::cout << std::endl;
            }
            
            // Check for lockfile
            fs::path lockfile = plugin_path.parent_path() / "spark.lock";
            if (fs::exists(lockfile)) {
                std::cout << termcolor::green << "  Lockfile: Present" << termcolor::reset << std::endl;
            } else {
                std::cout << termcolor::yellow << "  Lockfile: Missing" << termcolor::reset << std::endl;
            }
            
            std::cout << std::endl;
        }
    }
    
    // Show local installation info
    if (has_local) {
        std::cout << termcolor::bold << "Local Installation (./plugins/):" << termcolor::reset << std::endl;
        fs::path plugin_path = get_local_plugin_dir() / plugin_name / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_path);
        
        if (meta) {
            std::cout << "  Name: " << meta->name << std::endl;
            std::cout << "  Version: " << meta->version << std::endl;
            std::cout << "  Author: " << meta->author << std::endl;
            
            if (!meta->description.empty()) {
                std::cout << "  Description: " << meta->description << std::endl;
            }
            
            if (!meta->repository.empty()) {
                std::cout << "  Repository: " << meta->repository << std::endl;
            }
            
            if (meta->deprecated) {
                std::cout << termcolor::red << "  Status: DEPRECATED" << termcolor::reset << std::endl;
            }
            
            // Count versions
            fs::path versions_dir = get_local_plugin_dir() / plugin_name / "versions";
            if (fs::exists(versions_dir)) {
                int version_count = 0;
                std::cout << "  Versions: ";
                for (const auto& entry : fs::directory_iterator(versions_dir)) {
                    if (fs::is_directory(entry.path())) {
                        if (version_count > 0) std::cout << ", ";
                        std::cout << entry.path().filename().string();
                        version_count++;
                    }
                }
                std::cout << std::endl;
            }
            
            std::cout << std::endl;
        }
    }
    
    if (!has_global && !has_local) {
        std::cout << termcolor::yellow << "[*] Plugin not installed in any scope" << termcolor::reset << std::endl;
        std::cout << std::endl;
    }
    
    // Query the central registry
    std::cout << termcolor::bold << "Registry Information:" << termcolor::reset << std::endl;
    auto registry_entry = query_registry(plugin_name);
    
    if (registry_entry) {
        std::cout << "  Name: " << registry_entry->name << std::endl;
        std::cout << "  Repository: " << registry_entry->repository << std::endl;
        std::cout << termcolor::green << "  Status: Available in registry" << termcolor::reset << std::endl;
    } else {
        std::cout << termcolor::yellow << "  Status: Not found in central registry" << termcolor::reset << std::endl;
    }
    
    return 0;
}

} // namespace commands
} // namespace spark

#endif // SPARK_COMMANDS_INFO_HPP
