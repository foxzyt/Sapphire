#ifndef TOPAZ_COMMANDS_EXPAND_HPP
#define TOPAZ_COMMANDS_EXPAND_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include <iostream>
#include <filesystem>
#include "termcolor.hpp"

namespace topaz {
namespace commands {

// Version management - create version structure
inline int cmd_expand(const std::string& version, const std::filesystem::path& working_dir) {
    // Validate version format
    if (version.empty()) {
        std::cerr << termcolor::red << "[!] Version cannot be empty" << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz expand <version>" << std::endl;
        return 1;
    }
    
    // Check if PLUGIN.txt exists in root
    fs::path plugin_txt_path = working_dir / "PLUGIN.txt";
    if (!fs::exists(plugin_txt_path)) {
        std::cerr << termcolor::red << "[!] PLUGIN.txt not found in current directory" << termcolor::reset << std::endl;
        std::cerr << termcolor::yellow << "[!] Run 'topaz init' first to create a plugin base" << termcolor::reset << std::endl;
        return 1;
    }
    
    // Read the root PLUGIN.txt
    auto meta = parse_plugin_txt(plugin_txt_path);
    if (!meta) {
        std::cerr << termcolor::red << "[!] Failed to parse PLUGIN.txt" << termcolor::reset << std::endl;
        return 1;
    }
    
    // Update version in metadata
    meta->version = version;
    
    // Create version directory structure
    fs::path version_dir = working_dir / "versions" / ("v" + version);
    fs::path files_dir = version_dir / "files";
    
    try {
        fs::create_directories(files_dir);
        
        // Copy PLUGIN.txt to version directory
        fs::path version_plugin_txt = version_dir / "PLUGIN.txt";
        write_plugin_txt(version_plugin_txt, *meta);
        
        // Create empty DEPENDENCIES.txt
        fs::path deps_txt = version_dir / "DEPENDENCIES.txt";
        std::ofstream deps_file(deps_txt);
        deps_file.close();
        
        // Create default main.sp file
        fs::path main_sp = files_dir / "main.sp";
        std::ofstream main_file(main_sp);
        main_file << "// Main entry point for " << meta->name << " v" << version << "\n";
        main_file << "// Author: " << meta->author << "\n\n";
        main_file << "function main() {\n";
        main_file << "    print(\"Hello from " << meta->name << "!\");\n";
        main_file << "}\n";
        main_file << "main();\n";
        main_file.close();
        
        std::cout << termcolor::green << "[+] Version structure created: v" << version << termcolor::reset << std::endl;
        std::cout << termcolor::green << "[+] Files directory: " << files_dir << termcolor::reset << std::endl;
        
        // Interactive dependency addition
        std::vector<Dependency> dependencies;
        std::string response;
        
        while (true) {
            std::cout << "Add new dependency? (y/N): ";
            std::getline(std::cin, response);
            response = trim(response);
            
            if (response != "y" && response != "Y") {
                break;
            }
            
            std::string dep_name, dep_version;
            
            std::cout << "Dependency name: ";
            std::getline(std::cin, dep_name);
            dep_name = trim(dep_name);
            
            if (dep_name.empty()) {
                std::cout << termcolor::yellow << "[!] Skipping empty dependency name" << termcolor::reset << std::endl;
                continue;
            }
            
            std::cout << "Dependency version (default: latest): ";
            std::getline(std::cin, dep_version);
            dep_version = trim(dep_version);
            
            if (dep_version.empty()) {
                dep_version = "latest";
            }
            
            dependencies.emplace_back(dep_name, dep_version);
            std::cout << termcolor::green << "[+] Added dependency: " << dep_name << ":" << dep_version << termcolor::reset << std::endl;
        }
        
        // Write dependencies (create empty file if no dependencies)
        write_dependencies_txt(deps_txt, dependencies);
        if (!dependencies.empty()) {
            std::cout << termcolor::green << "[+] Dependencies written to DEPENDENCIES.txt" << termcolor::reset << std::endl;
        } else {
            std::cout << termcolor::green << "[+] Empty DEPENDENCIES.txt created" << termcolor::reset << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << termcolor::red << "[!] Failed to create version structure: " << e.what() << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_EXPAND_HPP
