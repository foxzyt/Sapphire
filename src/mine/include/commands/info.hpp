#ifndef MINE_COMMANDS_INFO_HPP
#define MINE_COMMANDS_INFO_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include <iostream>
#include <filesystem>

namespace mine {
namespace commands {

// Show detailed information about a plugin
inline int cmd_info(const std::string& plugin_name) {
    if (plugin_name.empty()) {
        std::cerr << "[!] Plugin name cannot be empty" << std::endl;
        std::cerr << "Usage: mine info <name>" << std::endl;
        return 1;
    }
    
    std::cout << "=== Plugin Information: " << plugin_name << " ===" << std::endl;
    std::cout << std::endl;
    
    // Check if plugin is installed locally
    if (is_plugin_installed(plugin_name)) {
        fs::path plugin_path = get_plugin_dir() / plugin_name / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_path);
        
        if (meta) {
            std::cout << "Local Installation:" << std::endl;
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
                std::cout << "  Status: DEPRECATED" << std::endl;
            }
            
            if (!meta->notice.empty()) {
                std::cout << "  Notice: " << meta->notice << std::endl;
            }
            
            // Count versions
            fs::path versions_dir = get_plugin_dir() / plugin_name / "versions";
            if (fs::exists(versions_dir)) {
                int version_count = 0;
                for (const auto& entry : fs::directory_iterator(versions_dir)) {
                    if (fs::is_directory(entry.path())) {
                        version_count++;
                    }
                }
                std::cout << "  Installed versions: " << version_count << std::endl;
            }
            
            std::cout << std::endl;
        }
    } else {
        std::cout << "[*] Plugin not installed locally" << std::endl;
        std::cout << std::endl;
    }
    
    // Query the central registry
    std::cout << "Registry Information:" << std::endl;
    auto registry_entry = query_registry(plugin_name);
    
    if (registry_entry) {
        std::cout << "  Name: " << registry_entry->name << std::endl;
        std::cout << "  Repository: " << registry_entry->repository << std::endl;
        std::cout << "  Status: Available in registry" << std::endl;
    } else {
        std::cout << "  Status: Not found in central registry" << std::endl;
    }
    
    return 0;
}

} // namespace commands
} // namespace mine

#endif // MINE_COMMANDS_INFO_HPP
