#ifndef SPARK_COMMANDS_LIST_HPP
#define SPARK_COMMANDS_LIST_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/sapphire_version.hpp"
#include "core/semver.hpp"
#include <iostream>
#include <filesystem>
#include <iomanip>
#include "termcolor.hpp"

namespace spark {
namespace commands {

// List installed plugins (global and local scope)
inline int cmd_list() {
    int total_plugins = 0;

    // -----------------------------------------------------------------------
    // Sapphire Runtime Banner
    // -----------------------------------------------------------------------
    {
        std::string active_ver = read_active_version();
        auto installed_vers    = get_installed_sapphire_versions();

        std::cout << termcolor::bold << termcolor::magenta
                  << "=== Sapphire Runtime ==="
                  << termcolor::reset << std::endl;

        if (active_ver.empty()) {
            std::cout << termcolor::yellow
                      << "  Runtime: (not installed — run: spark sapphire install latest)"
                      << termcolor::reset << std::endl;
        } else {
            std::cout << "  Active:  "
                      << termcolor::green << termcolor::bold
                      << semver::with_v(active_ver)
                      << termcolor::reset;

            if (installed_vers.size() > 1) {
                std::cout << "  ("
                          << installed_vers.size()
                          << " versions installed — run: spark sapphire versions)";
            }
            std::cout << std::endl;

            std::cout << "  Path:    "
                      << termcolor::cyan
                      << get_sapphire_bin_dir().string()
                      << termcolor::reset << std::endl;
        }
        std::cout << std::endl;
    }

    // List global plugins
    fs::path global_dir = get_plugin_dir();

    std::cout << termcolor::bold << "=== Global Plugins (AppData) ===" << termcolor::reset << std::endl;
    std::cout << std::endl;
    
    int global_count = 0;
    if (fs::exists(global_dir)) {
        for (const auto& entry : fs::directory_iterator(global_dir)) {
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
                // If it doesn't have a versions directory and no PLUGIN.txt, it's just garbage, skip silently.
                if (!fs::exists(plugin_txt)) {
                    continue;
                }
                std::cout << termcolor::red << plugin_name << " [INVALID: No versions]" << termcolor::reset << std::endl;
                global_count++;
                continue;
            }
            
            // Collect all versions
            std::vector<std::string> versions;
            for (const auto& version_entry : fs::directory_iterator(versions_dir)) {
                if (fs::is_directory(version_entry.path())) {
                    std::string version_dir = version_entry.path().filename().string();
                    if (version_dir.size() > 1 && version_dir[0] == 'v') {
                        versions.push_back(version_dir.substr(1));
                    } else {
                        versions.push_back(version_dir);
                    }
                }
            }
            
            if (versions.empty()) {
                std::cout << termcolor::yellow << plugin_name << " [No versions]" << termcolor::reset << std::endl;
                global_count++;
                continue;
            }
            
            // Format output: Name    Versions    (By: Author)
            std::cout << std::left << std::setw(30) << plugin_name;
            
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
            
            global_count++;
        }
    }
    
    if (global_count == 0) {
        std::cout << "(No global plugins installed)" << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "Total: " << global_count << " global plugin(s)" << std::endl;
    }
    
    total_plugins += global_count;
    
    // List local plugins if in a project
    if (is_sapphire_project()) {
        fs::path local_dir = get_local_plugin_dir();
        std::cout << std::endl;
        std::cout << termcolor::bold << "=== Local Plugins (./plugins/) ===" << termcolor::reset << std::endl;
        std::cout << std::endl;
        
        int local_count = 0;
        if (fs::exists(local_dir)) {
            for (const auto& entry : fs::directory_iterator(local_dir)) {
                if (!fs::is_directory(entry.path())) {
                    continue;
                }
                
                std::string plugin_name = entry.path().filename().string();
                
                // Check for PLUGIN.txt
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
                
                // Collect all versions
                fs::path versions_dir = entry.path() / "versions";
                if (!fs::exists(versions_dir) || !fs::is_directory(versions_dir)) {
                    if (!fs::exists(plugin_txt)) continue;
                }
                
                std::vector<std::string> versions;
                if (fs::exists(versions_dir)) {
                    for (const auto& version_entry : fs::directory_iterator(versions_dir)) {
                        if (fs::is_directory(version_entry.path())) {
                            std::string version_dir = version_entry.path().filename().string();
                            if (version_dir.size() > 1 && version_dir[0] == 'v') {
                                versions.push_back(version_dir.substr(1));
                            } else {
                                versions.push_back(version_dir);
                            }
                        }
                    }
                }
                
                std::string versions_str;
                if (!versions.empty()) {
                    for (size_t i = 0; i < versions.size(); i++) {
                        versions_str += "v" + versions[i];
                        if (i < versions.size() - 1) versions_str += ", ";
                    }
                } else {
                    versions_str = "no versions";
                }
                
                std::cout << std::left << std::setw(30) << plugin_name
                          << std::setw(20) << versions_str
                          << "(By: " << author << ")" << std::endl;
                
                if (!description.empty()) {
                    std::cout << std::left << std::setw(30) << "" 
                              << std::setw(20) << description << std::endl;
                }
                
                local_count++;
            }
        }
        
        if (local_count == 0) {
            std::cout << "(No local plugins installed)" << std::endl;
        } else {
            std::cout << std::endl;
            std::cout << "Total: " << local_count << " local plugin(s)" << std::endl;
        }
        
        total_plugins += local_count;
    }
    
    std::cout << std::endl;
    std::cout << termcolor::green << "Total: " << total_plugins << " plugin(s) installed" << termcolor::reset << std::endl;
    
    return 0;
}

} // namespace commands
} // namespace spark

#endif // SPARK_COMMANDS_LIST_HPP
