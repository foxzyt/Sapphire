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
        
        // Check for PLUGIN.txt
        fs::path plugin_txt = entry.path() / "PLUGIN.txt";
        if (!fs::exists(plugin_txt)) {
            std::cout << plugin_name << " [INVALID: Missing PLUGIN.txt]" << std::endl;
            plugin_count++;
            continue;
        }
        
        // Parse PLUGIN.txt
        auto meta = parse_plugin_txt(plugin_txt);
        if (!meta) {
            std::cout << plugin_name << " [INVALID: Corrupted PLUGIN.txt]" << std::endl;
            plugin_count++;
            continue;
        }
        
        // Format output: Name    Version    (By: Author)
        std::cout << std::left << std::setw(30) << meta->name 
                  << std::setw(12) << ("v" + meta->version)
                  << "(By: " << meta->author << ")" << std::endl;
        
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
