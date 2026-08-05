#ifndef TOPAZ_COMMANDS_TREE_HPP
#define TOPAZ_COMMANDS_TREE_HPP

#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/lockfile.hpp"
#include "termcolor.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// Helper: Check lock file status (forward declaration needed)
// ---------------------------------------------------------------------------
static std::string get_lock_status(const std::string& plugin_name, const std::string& version) {
    fs::path plugin_base = get_plugin_base_dir(plugin_name);
    fs::path version_dir = plugin_base / "versions" / ("v" + version);
    fs::path lockfile_path = version_dir / "topaz.lock";

    if (fs::exists(lockfile_path)) {
        return " [locked]";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Helper: Get dependencies for a specific version of a plugin
// ---------------------------------------------------------------------------
static std::vector<Dependency> get_version_dependencies(const std::string& plugin_name, const std::string& version) {
    fs::path plugin_base = get_plugin_base_dir(plugin_name);
    fs::path version_dir = plugin_base / "versions" / ("v" + version);

    if (fs::exists(version_dir)) {
        fs::path deps_path = version_dir / "DEPENDENCIES.txt";
        if (fs::exists(deps_path)) {
            return parse_dependencies_txt(deps_path);
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// Helper: Recursively print dependency tree
// ---------------------------------------------------------------------------
static void print_dependency_tree(const std::string& plugin_name, const std::string& version,
                                   std::set<std::string>& visited, const std::string& prefix,
                                   bool is_last, bool show_all_versions) {
    // Avoid circular dependencies
    std::string node_key = plugin_name + ":" + version;
    if (visited.find(node_key) != visited.end()) {
        std::cout << prefix;
        if (is_last) {
            std::cout << "└── ";
        } else {
            std::cout << "├── ";
        }
        std::cout << termcolor::yellow << plugin_name << " v" << version
                  << " (circular)" << termcolor::reset << std::endl;
        return;
    }

    visited.insert(node_key);

    std::cout << prefix;
    if (is_last) {
        std::cout << "└── ";
    } else {
        std::cout << "├── ";
    }

    std::cout << termcolor::green << plugin_name << " v" << version << termcolor::reset;

    // Show if it was found in lock file
    std::string lock_status = get_lock_status(plugin_name, version);

    std::cout << std::endl;

    // Get dependencies
    auto deps = get_version_dependencies(plugin_name, version);

    // Also check if we need to show other installed versions
    if (show_all_versions && version != "latest") {
        auto all_versions = get_plugin_versions(plugin_name);
        for (const auto& ver : all_versions) {
            if (ver != version) {
                std::cout << prefix;
                if (is_last) {
                    std::cout << "    ";
                } else {
                    std::cout << "│   ";
                }
                std::cout << "├── " << termcolor::cyan << plugin_name << " v" << ver
                          << " (also installed)" << termcolor::reset << std::endl;
            }
        }
    }

    // Print dependencies recursively
    for (size_t i = 0; i < deps.size(); i++) {
        std::string new_prefix = prefix;
        if (is_last) {
            new_prefix += "    ";
        } else {
            new_prefix += "│   ";
        }

        std::string dep_version = deps[i].version;
        std::string dep_name = deps[i].name;

        // Resolve "latest" to actual version
        if (dep_version == "latest") {
            auto installed_versions = get_plugin_versions(dep_name);
            if (!installed_versions.empty()) {
                dep_version = installed_versions.back();
            }
        }

        // Check if dependency is installed
        if (!is_plugin_installed_anywhere(dep_name)) {
            std::cout << new_prefix;
            if (i == deps.size() - 1) {
                std::cout << "└── ";
            } else {
                std::cout << "├── ";
            }
            std::cout << termcolor::red << dep_name << " v" << dep_version
                      << " (NOT INSTALLED)" << termcolor::reset << std::endl;
            continue;
        }

        print_dependency_tree(dep_name, dep_version, visited, new_prefix,
                              i == deps.size() - 1, false);
    }
}

// ---------------------------------------------------------------------------
// cmd_tree — Display plugin dependency tree
//
// Usage:
//   topaz tree                      — Show tree for all installed plugins
//   topaz tree <name>               — Show dependency tree for specific plugin
//   topaz tree <name> <version>     — Show tree for specific version
// ---------------------------------------------------------------------------
static nlohmann::json build_json_tree(const std::string& plugin_name, const std::string& version, std::set<std::string>& visited) {
    nlohmann::json j;
    j["name"] = plugin_name;
    j["version"] = version;
    j["dependencies"] = nlohmann::json::array();
    
    std::string node_key = plugin_name + ":" + version;
    if (visited.find(node_key) != visited.end()) {
        j["circular"] = true;
        return j;
    }
    visited.insert(node_key);
    
    auto deps = get_version_dependencies(plugin_name, version);
    for (const auto& dep : deps) {
        std::string dep_ver = dep.version;
        if (dep_ver == "latest") {
            auto installed = get_plugin_versions(dep.name);
            if (!installed.empty()) dep_ver = installed.back();
        }
        j["dependencies"].push_back(build_json_tree(dep.name, dep_ver, visited));
    }
    return j;
}

static void print_mermaid_tree(const std::string& plugin_name, const std::string& version, 
                               std::set<std::string>& visited, std::set<std::string>& printed_edges) {
    std::string node_key = plugin_name + ":" + version;
    if (visited.find(node_key) != visited.end()) return;
    visited.insert(node_key);
    
    auto deps = get_version_dependencies(plugin_name, version);
    if (deps.empty()) {
        std::string node_def = plugin_name + "[\"" + plugin_name + " v" + version + "\"]";
        if (printed_edges.find(node_def) == printed_edges.end()) {
            printed_edges.insert(node_def);
            std::cout << "    " << node_def << std::endl;
        }
    }
    
    for (const auto& dep : deps) {
        std::string dep_ver = dep.version;
        if (dep_ver == "latest") {
            auto installed = get_plugin_versions(dep.name);
            if (!installed.empty()) dep_ver = installed.back();
        }
        std::string edge = plugin_name + "-->" + dep.name;
        if (printed_edges.find(edge) == printed_edges.end()) {
            printed_edges.insert(edge);
            std::cout << "    " << plugin_name << "[\"" << plugin_name << " v" << version << "\"] --> " 
                      << dep.name << "[\"" << dep.name << " v" << dep_ver << "\"]" << std::endl;
        }
        print_mermaid_tree(dep.name, dep_ver, visited, printed_edges);
    }
}

inline int cmd_tree(const std::string& plugin_name = "", const std::string& version = "latest", const std::string& format = "text") {
    if (format == "mermaid") {
        std::cout << "graph TD" << std::endl;
        std::set<std::string> visited;
        std::set<std::string> printed_edges;
        if (plugin_name.empty()) {
            // Collect all installed plugins
            std::vector<std::string> all_plugins;
            std::set<std::string> seen;
            auto add_plugins = [&](const fs::path& dir) {
                if (fs::exists(dir)) {
                    for (const auto& entry : fs::directory_iterator(dir)) {
                        if (fs::is_directory(entry.path())) {
                            std::string name = entry.path().filename().string();
                            if (name != ".cache" && seen.find(name) == seen.end()) {
                                seen.insert(name);
                                all_plugins.push_back(name);
                            }
                        }
                    }
                }
            };
            add_plugins(get_plugin_dir());
            if (is_sapphire_project()) add_plugins(get_local_plugin_dir());
            
            for (const auto& name : all_plugins) {
                auto versions = get_plugin_versions(name);
                if (!versions.empty()) {
                    print_mermaid_tree(name, versions.back(), visited, printed_edges);
                }
            }
        } else {
            std::string resolved_version = version;
            if (version == "latest") {
                auto versions = get_plugin_versions(plugin_name);
                if (!versions.empty()) resolved_version = versions.back();
            }
            print_mermaid_tree(plugin_name, resolved_version, visited, printed_edges);
        }
        return 0;
    }
    
    if (format == "json") {
        nlohmann::json root_json = nlohmann::json::array();
        std::set<std::string> visited;
        if (plugin_name.empty()) {
            std::vector<std::string> all_plugins;
            std::set<std::string> seen;
            auto add_plugins = [&](const fs::path& dir) {
                if (fs::exists(dir)) {
                    for (const auto& entry : fs::directory_iterator(dir)) {
                        if (fs::is_directory(entry.path())) {
                            std::string name = entry.path().filename().string();
                            if (name != ".cache" && seen.find(name) == seen.end()) {
                                seen.insert(name);
                                all_plugins.push_back(name);
                            }
                        }
                    }
                }
            };
            add_plugins(get_plugin_dir());
            if (is_sapphire_project()) add_plugins(get_local_plugin_dir());
            
            for (const auto& name : all_plugins) {
                auto versions = get_plugin_versions(name);
                if (!versions.empty()) {
                    root_json.push_back(build_json_tree(name, versions.back(), visited));
                }
            }
        } else {
            std::string resolved_version = version;
            if (version == "latest") {
                auto versions = get_plugin_versions(plugin_name);
                if (!versions.empty()) resolved_version = versions.back();
            }
            root_json = build_json_tree(plugin_name, resolved_version, visited);
        }
        std::cout << root_json.dump(2) << std::endl;
        return 0;
    }

    if (plugin_name.empty()) {
        // Show tree for all installed plugins
        std::cout << termcolor::bold << "Topaz Dependency Tree" << termcolor::reset << std::endl;
        std::cout << termcolor::cyan << "Showing all installed plugins" << termcolor::reset << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        // Collect all installed plugins
        std::vector<std::string> all_plugins;
        std::set<std::string> seen;

        auto add_plugins = [&](const fs::path& dir) {
            if (fs::exists(dir)) {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (fs::is_directory(entry.path())) {
                        std::string name = entry.path().filename().string();
                        if (name != ".cache" && seen.find(name) == seen.end()) {
                            seen.insert(name);
                            all_plugins.push_back(name);
                        }
                    }
                }
            }
        };

        add_plugins(get_plugin_dir());
        if (is_sapphire_project()) {
            add_plugins(get_local_plugin_dir());
        }

        if (all_plugins.empty()) {
            std::cout << termcolor::yellow << "(No plugins installed)" << termcolor::reset << std::endl;
            return 0;
        }

        for (size_t i = 0; i < all_plugins.size(); i++) {
            std::string name = all_plugins[i];
            auto versions = get_plugin_versions(name);

            if (!versions.empty()) {
                std::string latest = versions.back();
                std::set<std::string> visited;

                std::cout << std::endl;
                if (i == all_plugins.size() - 1) {
                    std::cout << "└─ ";
                } else {
                    std::cout << "├─ ";
                }
                std::cout << termcolor::bold << name << " v" << latest << termcolor::reset;

                // Check for lock file
                fs::path base = get_plugin_base_dir(name);
                fs::path lock_path = base / "versions" / ("v" + latest) / "topaz.lock";
                if (fs::exists(lock_path)) {
                    std::cout << termcolor::green << " [locked]" << termcolor::reset;
                }

                std::cout << std::endl;

                // Show dependencies for the latest version
                auto deps = get_version_dependencies(name, latest);
                for (size_t j = 0; j < deps.size(); j++) {
                    std::string dep_name = deps[j].name;
                    std::string dep_version = deps[j].version;

                    if (dep_version == "latest") {
                        auto installed_versions = get_plugin_versions(dep_name);
                        if (!installed_versions.empty()) {
                            dep_version = installed_versions.back();
                        }
                    }

                    print_dependency_tree(dep_name, dep_version, visited,
                                          "    ",
                                          j == deps.size() - 1, false);
                }
            }
        }

        std::cout << std::endl;
        std::cout << termcolor::green << "Total: " << all_plugins.size() << " plugin(s)" << termcolor::reset << std::endl;

    } else {
        // Show tree for a specific plugin
        if (!is_plugin_installed_anywhere(plugin_name)) {
            std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' is not installed." << termcolor::reset << std::endl;
            return 1;
        }

        std::string resolved_version = version;
        if (version == "latest") {
            auto versions = get_plugin_versions(plugin_name);
            if (!versions.empty()) {
                resolved_version = versions.back();
            }
        }

        std::cout << termcolor::bold << "Dependency Tree: " << plugin_name << " v" << resolved_version << termcolor::reset << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        // Get lock status
        fs::path plugin_base = get_plugin_base_dir(plugin_name);
        fs::path version_dir = plugin_base / "versions" / ("v" + resolved_version);
        fs::path lockfile_path = version_dir / "topaz.lock";

        if (fs::exists(lockfile_path)) {
            if (topaz::g_verbose) std::cout << termcolor::green << "[*] Lock file present" << termcolor::reset << std::endl;
        } else {
            if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] No lock file (run: topaz lock " << plugin_name << ")" << termcolor::reset << std::endl;
        }
        std::cout << std::endl;

        // Print the root node and its tree
        std::cout << termcolor::bold << plugin_name << termcolor::reset;
        std::cout << " v" << resolved_version << std::endl;

        std::set<std::string> visited;
        auto deps = get_version_dependencies(plugin_name, resolved_version);

        for (size_t i = 0; i < deps.size(); i++) {
            std::string dep_name = deps[i].name;
            std::string dep_version = deps[i].version;

            if (dep_version == "latest") {
                auto installed_versions = get_plugin_versions(dep_name);
                if (!installed_versions.empty()) {
                    dep_version = installed_versions.back();
                }
            }

            print_dependency_tree(dep_name, dep_version, visited, "",
                                  i == deps.size() - 1, false);
        }

        if (deps.empty()) {
            std::cout << "  (no dependencies)" << std::endl;
        }
    }

    return 0;
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_TREE_HPP