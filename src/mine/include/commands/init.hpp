#ifndef MINE_COMMANDS_INIT_HPP
#define MINE_COMMANDS_INIT_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include <iostream>
#include <filesystem>
#include "termcolor.hpp"

namespace mine {
namespace commands {

// Interactive plugin creation
inline int cmd_init(const std::filesystem::path& working_dir) {
    PluginMeta meta;
    
    std::cout << "=== Mine Plugin Initialization ===" << std::endl;
    std::cout << "This will create a new plugin structure in the current directory." << std::endl;
    std::cout << std::endl;
    
    // Prompt for plugin name
    std::cout << "Plugin name: ";
    std::getline(std::cin, meta.name);
    meta.name = trim(meta.name);
    
    if (meta.name.empty()) {
        std::cerr << termcolor::red << "[!] Plugin name cannot be empty" << termcolor::reset << std::endl;
        return 1;
    }
    
    // Prompt for author
    std::cout << "Author: ";
    std::getline(std::cin, meta.author);
    meta.author = trim(meta.author);
    
    if (meta.author.empty()) {
        std::cerr << termcolor::red << "[!] Author cannot be empty" << termcolor::reset << std::endl;
        return 1;
    }
    
    // Prompt for description
    std::cout << "Description (optional): ";
    std::getline(std::cin, meta.description);
    meta.description = trim(meta.description);
    
    // Create plugin directory with the plugin name
    fs::path plugin_dir = working_dir / meta.name;
    fs::create_directories(plugin_dir);
    
    // Create versions directory
    fs::path versions_dir = plugin_dir / "versions";
    fs::create_directories(versions_dir);
    
    // Create PLUGIN.txt in the plugin directory
    fs::path plugin_txt_path = plugin_dir / "PLUGIN.txt";
    
    if (write_plugin_txt(plugin_txt_path, meta)) {
        std::cout << termcolor::green << "[+] Plugin directory created: " << plugin_dir << termcolor::reset << std::endl;
        std::cout << termcolor::green << "[+] Use 'mine expand <version>' to add code and version structure." << termcolor::reset << std::endl;
        return 0;
    } else {
        std::cerr << termcolor::red << "[!] Failed to create PLUGIN.txt" << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace mine

#endif // MINE_COMMANDS_INIT_HPP
