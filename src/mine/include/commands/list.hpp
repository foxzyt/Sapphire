#ifndef MINE_COMMANDS_LIST_HPP
#define MINE_COMMANDS_LIST_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace mine {
namespace commands {

// List installed plugins
inline int cmd_list() {
    fs::path plugin_dir = get_plugin_dir();
    
    if (!fs::exists(plugin_dir)) {
        std::cout << "[*] No plugins directory found" << std::endl;
        return 0;
    }
    
    std::cout << "=== Installed Plugins ===" << std::endl;
    std::cout << std::endl;
    
    int plugin_count = 0;
    
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
        
        // Check for PLUGIN.txt in base (for metadata only, not version)
        fs::path plugin_txt = entry.path() / "PLUGIN.txt";
        std::string author = "Unknown";
        std::string description = "";
        
        if (fs::exists(plugin_txt)) {
            auto meta = parse_plugin_txt(plugin_txt);
            if (meta) {
                author = meta->author;
                description = meta->description;
            }
        }
        
        // Check for versions directory
        fs::path versions_dir = entry.path() / "versions";
        if (!fs::exists(versions_dir) || !fs::is_directory(versions_dir)) {
            std::cout << plugin_name << " [INVALID: No versions directory]" << std::endl;
            plugin_count++;
            continue;
        }
        
        // Collect all versions
        std::vector<std::string> versions;
        for (const auto& version_entry : fs::directory_iterator(versions_dir)) {
            if (fs::is_directory(version_entry.path())) {
                std::string version_dir = version_entry.path().filename().string();
                // Remove 'v' prefix if present
                if (version_dir.size() > 1 && version_dir[0] == 'v') {
                    versions.push_back(version_dir.substr(1));
                } else {
                    versions.push_back(version_dir);
                }
            }
        }
        
        if (versions.empty()) {
            std::cout << plugin_name << " [No versions installed]" << std::endl;
            plugin_count++;
            continue;
        }
        
        // Format output: Name    Versions    (By: Author)
        std::cout << std::left << std::setw(30) << plugin_name;
        
        // Show all versions
        std::string versions_str;
        for (size_t i = 0; i < versions.size(); i++) {
            versions_str += "v" + versions[i];
            if (i < versions.size() - 1) versions_str += ", ";
        }
        
        std::cout << std::setw(20) << versions_str
                  << "(By: " << author << ")" << std::endl;
        
        if (!description.empty()) {
            std::cout << std::left << std::setw(30) << "" 
                      << std::setw(20) << description << std::endl;
        }
        
        plugin_count++;
    }
    
    if (plugin_count == 0) {
        std::cout << "(No plugins installed yet)" << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "Total: " << plugin_count << " plugin(s)" << std::endl;
    }
    
    return 0;
}

} // namespace commands
} // namespace mine

#endif // MINE_COMMANDS_LIST_HPP
