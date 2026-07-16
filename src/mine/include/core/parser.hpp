#ifndef MINE_PARSER_HPP
#define MINE_PARSER_HPP

#include "types.hpp"
#include "fs_utils.hpp"
#include <fstream>
#include <sstream>
#include <optional>

namespace mine {

// Parse PLUGIN.txt file
// Returns std::optional<PluginMeta> - empty if file doesn't exist or parsing fails
inline std::optional<PluginMeta> parse_plugin_txt(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    PluginMeta meta;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the first ':' to split key and value
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue; // Invalid line format
        }

        std::string key = trim(line.substr(0, colon_pos));
        std::string value = trim(line.substr(colon_pos + 1));

        // Map keys to struct fields
        if (key == "name") {
            meta.name = value;
        } else if (key == "author") {
            meta.author = value;
        } else if (key == "version") {
            meta.version = value;
        } else if (key == "description") {
            meta.description = value;
        } else if (key == "repository") {
            meta.repository = value;
        } else if (key == "deprecated") {
            meta.deprecated = (value == "true" || value == "yes" || value == "1");
        } else if (key == "notice") {
            meta.notice = value;
        }
    }

    if (meta.is_valid()) {
        return meta;
    }
    return std::nullopt;
}

// Parse DEPENDENCIES.txt file
// Returns vector of Dependency objects
inline std::vector<Dependency> parse_dependencies_txt(const fs::path& path) {
    std::vector<Dependency> dependencies;

    if (!fs::exists(path)) {
        return dependencies; // Return empty vector if file doesn't exist
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return dependencies;
    }

    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the first ':' to split name and version
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            // If no colon, assume "latest" version
            dependencies.emplace_back(line, "latest");
        } else {
            std::string name = trim(line.substr(0, colon_pos));
            std::string version = trim(line.substr(colon_pos + 1));
            dependencies.emplace_back(name, version);
        }
    }

    return dependencies;
}

// Write PLUGIN.txt file
inline bool write_plugin_txt(const fs::path& path, const PluginMeta& meta) {
    fs::create_directories(path.parent_path());
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << "name: " << meta.name << "\n";
    file << "author: " << meta.author << "\n";
    file << "version: " << meta.version << "\n";
    
    if (!meta.description.empty()) {
        file << "description: " << meta.description << "\n";
    }
    
    if (!meta.repository.empty()) {
        file << "repository: " << meta.repository << "\n";
    }
    
    if (meta.deprecated) {
        file << "deprecated: true\n";
    }
    
    if (!meta.notice.empty()) {
        file << "notice: " << meta.notice << "\n";
    }

    return true;
}

// Write DEPENDENCIES.txt file
inline bool write_dependencies_txt(const fs::path& path, const std::vector<Dependency>& dependencies) {
    fs::create_directories(path.parent_path());
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& dep : dependencies) {
        file << dep.name << ": " << dep.version << "\n";
    }

    return true;
}

} // namespace mine

#endif // MINE_PARSER_HPP
