#ifndef MINE_FS_UTILS_HPP
#define MINE_FS_UTILS_HPP

#include <filesystem>
#include <string>
#include <stdexcept>
#include <iostream>
#include <set>
#include <vector>
#include <algorithm>

namespace mine {

namespace fs = std::filesystem;

// Get the global plugin directory: %APPDATA%/Sapphire/plugins/
inline fs::path get_plugin_dir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) {
        throw std::runtime_error("Could not access APPDATA environment variable");
    }
    fs::path plugin_dir = fs::path(appdata) / "Sapphire" / "plugins";
    fs::create_directories(plugin_dir);
    return plugin_dir;
#else
    // For non-Windows systems, use ~/.local/share/Sapphire/plugins/
    const char* home = std::getenv("HOME");
    if (!home) {
        throw std::runtime_error("Could not access HOME environment variable");
    }
    fs::path plugin_dir = fs::path(home) / ".local" / "share" / "Sapphire" / "plugins";
    fs::create_directories(plugin_dir);
    return plugin_dir;
#endif
}

// Get the local project plugin directory: ./plugins/ (relative to current working directory)
inline fs::path get_local_plugin_dir() {
    fs::path local_dir = fs::current_path() / "plugins";
    fs::create_directories(local_dir);
    return local_dir;
}

// Check if current directory is a Sapphire project (has main.sp or .sapphire file)
inline bool is_sapphire_project() {
    fs::path cwd = fs::current_path();
    if (fs::exists(cwd / "main.sp") || fs::exists(cwd / ".sapphire") || 
        fs::exists(cwd / "sapphire.json") || fs::exists(cwd / "PLUGIN.txt")) {
        return true;
    }
    // Check if there's a plugins/ directory already
    if (fs::exists(cwd / "plugins") && fs::is_directory(cwd / "plugins")) {
        return true;
    }
    return false;
}

// Get cache directory: %APPDATA%/Sapphire/plugins/.cache/
inline fs::path get_cache_dir() {
    fs::path cache_dir = get_plugin_dir() / ".cache";
    fs::create_directories(cache_dir);
    return cache_dir;
}

// Trim helper: removes spaces, tabs, and carriage returns
inline std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Check if a plugin is installed globally
inline bool is_plugin_installed(const std::string& plugin_name) {
    fs::path plugin_path = get_plugin_dir() / plugin_name;
    return fs::exists(plugin_path) && fs::is_directory(plugin_path);
}

// Check if a plugin is installed locally in the project's plugins/ directory
inline bool is_plugin_installed_local(const std::string& plugin_name) {
    fs::path plugin_path = get_local_plugin_dir() / plugin_name;
    return fs::exists(plugin_path) && fs::is_directory(plugin_path);
}

// Check if a plugin is installed either locally or globally (local first)
inline bool is_plugin_installed_anywhere(const std::string& plugin_name) {
    return is_plugin_installed_local(plugin_name) || is_plugin_installed(plugin_name);
}

// Get the best plugin directory (local takes priority over global)
inline fs::path get_best_plugin_dir(const std::string& plugin_name = "") {
    if (is_sapphire_project()) {
        fs::path local = get_local_plugin_dir();
        if (plugin_name.empty()) {
            return local;
        }
        if (fs::exists(local / plugin_name)) {
            return local;
        }
    }
    return get_plugin_dir();
}

// Get the path to a specific version of a plugin (checks local first, then global)
inline fs::path get_plugin_version_path(const std::string& plugin_name, const std::string& version) {
    // Check local first
    if (is_sapphire_project()) {
        fs::path local_path = get_local_plugin_dir() / plugin_name / "versions" / ("v" + version);
        if (fs::exists(local_path)) {
            return local_path;
        }
    }
    // Fall back to global
    return get_plugin_dir() / plugin_name / "versions" / ("v" + version);
}

// Get the base directory where a plugin is installed (local or global)
inline fs::path get_plugin_base_dir(const std::string& plugin_name) {
    if (is_plugin_installed_local(plugin_name)) {
        return get_local_plugin_dir() / plugin_name;
    }
    return get_plugin_dir() / plugin_name;
}

// Get all available versions for a plugin (local + global merged)
inline std::vector<std::string> get_plugin_versions(const std::string& plugin_name) {
    std::vector<std::string> versions;
    std::set<std::string> seen;
    
    auto add_versions = [&](const fs::path& base_dir) {
        fs::path versions_dir = base_dir / "versions";
        if (fs::exists(versions_dir) && fs::is_directory(versions_dir)) {
            for (const auto& entry : fs::directory_iterator(versions_dir)) {
                if (fs::is_directory(entry.path())) {
                    std::string version_name = entry.path().filename().string();
                    // Remove 'v' prefix if present
                    if (version_name.size() > 0 && version_name[0] == 'v') {
                        version_name = version_name.substr(1);
                    }
                    if (seen.find(version_name) == seen.end()) {
                        seen.insert(version_name);
                        versions.push_back(version_name);
                    }
                }
            }
        }
    };
    
    // Check local first
    if (is_sapphire_project()) {
        add_versions(get_local_plugin_dir());
    }
    // Then global
    add_versions(get_plugin_dir());
    
    return versions;
}

// Clean up cache directory
inline void cleanup_cache() {
    fs::path cache_dir = get_cache_dir();
    if (fs::exists(cache_dir)) {
        fs::remove_all(cache_dir);
        fs::create_directories(cache_dir);
    }
}

} // namespace mine

#endif // MINE_FS_UTILS_HPP
