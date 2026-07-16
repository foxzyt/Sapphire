#ifndef MINE_FS_UTILS_HPP
#define MINE_FS_UTILS_HPP

#include <filesystem>
#include <string>
#include <stdexcept>

namespace mine {

namespace fs = std::filesystem;

// Get the plugin directory: %APPDATA%/Sapphire/plugins/
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

// Check if a plugin is installed locally
inline bool is_plugin_installed(const std::string& plugin_name) {
    fs::path plugin_path = get_plugin_dir() / plugin_name;
    return fs::exists(plugin_path) && fs::is_directory(plugin_path);
}

// Get the path to a specific version of a plugin
inline fs::path get_plugin_version_path(const std::string& plugin_name, const std::string& version) {
    return get_plugin_dir() / plugin_name / "versions" / ("v" + version);
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
