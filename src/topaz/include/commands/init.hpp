#ifndef TOPAZ_COMMANDS_INIT_HPP
#define TOPAZ_COMMANDS_INIT_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include <iostream>
#include <filesystem>
#include "termcolor.hpp"

namespace topaz {
namespace commands {

// Interactive plugin creation
// Interactive plugin/project initialization
inline bool write_sapphire_json(const fs::path& path, const std::string& name, const std::string& version, const std::string& author, const std::string& description) {
    try {
        nlohmann::json j;
        j["name"] = name;
        j["version"] = version;
        j["author"] = author;
        j["description"] = description;
        j["sapphire"] = "^1.0.5";
        j["dependencies"] = nlohmann::json::object();
        j["devDependencies"] = nlohmann::json::object();
        
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        file.close();
        return true;
    } catch (...) {
        return false;
    }
}

inline int cmd_init(const std::filesystem::path& working_dir,
                    std::string name_opt = "",
                    std::string author_opt = "",
                    std::string desc_opt = "",
                    std::string ver_opt = "",
                    bool skip_prompts = false) {
    PluginMeta meta;
    
    if (skip_prompts) {
        meta.name = name_opt.empty() ? working_dir.filename().string() : name_opt;
        meta.author = author_opt.empty() ? "Author" : author_opt;
        meta.description = desc_opt;
        meta.version = ver_opt.empty() ? "1.0.0" : ver_opt;
    } else {
        std::cout << "=== Topaz Project/Plugin Initialization ===" << std::endl;
        std::cout << "This will create a new project/plugin structure in the current directory." << std::endl;
        std::cout << std::endl;
        
        // Prompt for name
        if (name_opt.empty()) {
            std::cout << "Name [" << working_dir.filename().string() << "]: ";
            std::string input;
            std::getline(std::cin, input);
            input = trim(input);
            meta.name = input.empty() ? working_dir.filename().string() : input;
        } else {
            meta.name = name_opt;
        }
        
        // Prompt for version
        if (ver_opt.empty()) {
            std::cout << "Version [1.0.0]: ";
            std::string input;
            std::getline(std::cin, input);
            input = trim(input);
            meta.version = input.empty() ? "1.0.0" : input;
        } else {
            meta.version = ver_opt;
        }
        
        // Prompt for author
        if (author_opt.empty()) {
            std::cout << "Author [Author]: ";
            std::string input;
            std::getline(std::cin, input);
            input = trim(input);
            meta.author = input.empty() ? "Author" : input;
        } else {
            meta.author = author_opt;
        }
        
        // Prompt for description
        if (desc_opt.empty()) {
            std::cout << "Description (optional): ";
            std::getline(std::cin, meta.description);
            meta.description = trim(meta.description);
        } else {
            meta.description = desc_opt;
        }
    }
    
    if (meta.name.empty()) {
        std::cerr << termcolor::red << "[!] Name cannot be empty" << termcolor::reset << std::endl;
        return 1;
    }
    
    // Create directory
    fs::path plugin_dir = working_dir / meta.name;
    fs::create_directories(plugin_dir);
    
    // Create versions directory
    fs::path versions_dir = plugin_dir / "versions";
    fs::create_directories(versions_dir);
    
    // Create PLUGIN.txt and sapphire.json in the directory
    fs::path plugin_txt_path = plugin_dir / "PLUGIN.txt";
    fs::path sapphire_json_path = plugin_dir / "sapphire.json";
    
    bool p_txt = write_plugin_txt(plugin_txt_path, meta);
    bool s_json = write_sapphire_json(sapphire_json_path, meta.name, meta.version, meta.author, meta.description);
    
    if (p_txt && s_json) {
        if (topaz::g_verbose) std::cout << termcolor::green << "[+] Project structure initialized: " << plugin_dir << termcolor::reset << std::endl;
        if (topaz::g_verbose) std::cout << termcolor::green << "[+] Generated sapphire.json and PLUGIN.txt" << termcolor::reset << std::endl;
        if (topaz::g_verbose) std::cout << termcolor::green << "[+] Use 'topaz expand <version>' to add code versioning." << termcolor::reset << std::endl;
        return 0;
    } else {
        std::cerr << termcolor::red << "[!] Failed to generate project files" << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_INIT_HPP
