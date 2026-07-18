#ifndef SPARK_COMMANDS_CHECK_HPP
#define SPARK_COMMANDS_CHECK_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/semver.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include "termcolor.hpp"

namespace spark {
namespace commands {

// Helper: check a single plugin directory for diagnostics
static void check_plugin_dir(const fs::path& plugin_dir, const std::string& scope_label,
                              int& total_plugins, int& errors, int& warnings) {
    if (!fs::exists(plugin_dir)) return;

    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (!fs::is_directory(entry.path())) continue;

        std::string plugin_name = entry.path().filename().string();

        // Skip cache directory
        if (plugin_name == ".cache") continue;

        total_plugins++;

        std::cout << "Plugin: " << plugin_name << " (" << scope_label << ")" << std::endl;

        // Check for PLUGIN.txt
        fs::path plugin_txt = entry.path() / "PLUGIN.txt";
        if (!fs::exists(plugin_txt)) {
            std::cout << termcolor::red << "  [!] ERROR: Missing PLUGIN.txt" << termcolor::reset << std::endl;
            errors++;
            continue;
        }

        // Parse PLUGIN.txt
        auto meta = parse_plugin_txt(plugin_txt);
        if (!meta) {
            std::cout << termcolor::red << "  [!] ERROR: Failed to parse PLUGIN.txt" << termcolor::reset << std::endl;
            errors++;
            continue;
        }

        std::cout << "  Version: " << meta->version << std::endl;
        std::cout << "  Author: " << meta->author << std::endl;

        // Check for deprecation
        if (meta->deprecated) {
            std::cout << termcolor::yellow << "  [!] WARNING: Plugin is deprecated" << termcolor::reset << std::endl;
            warnings++;
        }

        // Check for notices
        if (!meta->notice.empty()) {
            std::cout << termcolor::cyan << "  [i] NOTICE: " << meta->notice << termcolor::reset << std::endl;
        }

        // Check versions directory
        fs::path versions_dir = entry.path() / "versions";
        if (!fs::exists(versions_dir)) {
            std::cout << termcolor::red << "  [!] ERROR: Missing versions directory" << termcolor::reset << std::endl;
            errors++;
            continue;
        }

        // Check for at least one version
        int version_count = 0;
        for (const auto& version_entry : fs::directory_iterator(versions_dir)) {
            if (fs::is_directory(version_entry.path())) {
                version_count++;
            }
        }

        if (version_count == 0) {
            std::cout << termcolor::red << "  [!] ERROR: No versions found" << termcolor::reset << std::endl;
            errors++;
        } else {
            std::cout << "  Versions: " << version_count << std::endl;
        }

        // Check dependencies for the latest version
        if (version_count > 0) {
            fs::path latest_version_path = versions_dir / ("v" + meta->version);
            if (fs::exists(latest_version_path)) {
                fs::path deps_txt = latest_version_path / "DEPENDENCIES.txt";

                if (fs::exists(deps_txt)) {
                    auto dependencies = parse_dependencies_txt(deps_txt);

                    if (!dependencies.empty()) {
                        std::cout << "  Dependencies: " << dependencies.size() << std::endl;

                        // Check if dependencies are installed (anywhere: local or global)
                        for (const auto& dep : dependencies) {
                            if (!is_plugin_installed_anywhere(dep.name)) {
                                std::cout << termcolor::red << "    [!] MISSING: " << dep.name << termcolor::reset << " (required: " << dep.version << ")" << std::endl;
                                errors++;
                                continue;
                            }

                            // Check if the required version is actually available
                            std::string required_ver = dep.version;
                            std::vector<std::string> installed_versions = get_plugin_versions(dep.name);

                            if (required_ver == "latest") {
                                // "latest" means any version is acceptable
                                if (installed_versions.empty()) {
                                    std::cout << termcolor::red << "    [!] MISSING: " << dep.name << termcolor::reset << " (required: latest, but no versions installed)" << std::endl;
                                    errors++;
                                } else {
                                    std::string highest = installed_versions.back();
                                    std::cout << termcolor::green << "    [OK] " << dep.name << termcolor::reset << " (required: latest, installed: v" << highest << ")" << std::endl;
                                }
                            } else {
                                // Specific version requirement - check if that version exists
                                std::string clean_required = required_ver;
                                if (clean_required.size() > 0 && clean_required[0] == 'v') {
                                    clean_required = clean_required.substr(1);
                                }

                                bool version_found = false;
                                for (const auto& ver : installed_versions) {
                                    std::string clean_ver = ver;
                                    if (clean_ver.size() > 0 && clean_ver[0] == 'v') {
                                        clean_ver = clean_ver.substr(1);
                                    }
                                    if (clean_ver == clean_required) {
                                        version_found = true;
                                        break;
                                    }
                                }

                                if (version_found) {
                                    std::cout << termcolor::green << "    [OK] " << dep.name << termcolor::reset << " (required: " << required_ver << ")" << std::endl;
                                } else {
                                    std::string available;
                                    for (size_t vi = 0; vi < installed_versions.size(); vi++) {
                                        if (vi > 0) available += ", ";
                                        available += "v" + installed_versions[vi];
                                    }
                                    std::cout << termcolor::yellow << "    [!] WARNING: " << dep.name << termcolor::reset
                                              << " (required: " << required_ver << ", available: " << available << ")" << std::endl;
                                    warnings++;
                                }
                            }
                        }
                    }
                }
            }
        }

        std::cout << std::endl;
    }
}

// Diagnostic command - check plugin integrity and dependencies
inline int cmd_check() {
    std::cout << "=== Plugin Diagnostic Report ===" << std::endl;
    std::cout << std::endl;

    int total_plugins = 0;
    int errors = 0;
    int warnings = 0;

    // Check global plugins
    fs::path global_dir = get_plugin_dir();
    if (fs::exists(global_dir)) {
        std::cout << termcolor::bold << "--- Global Plugins (AppData) ---" << termcolor::reset << std::endl;
        check_plugin_dir(global_dir, "global", total_plugins, errors, warnings);
    }

    // Check local plugins if in a project
    if (is_sapphire_project()) {
        fs::path local_dir = get_local_plugin_dir();
        if (fs::exists(local_dir)) {
            std::cout << termcolor::bold << "--- Local Plugins (./plugins/) ---" << termcolor::reset << std::endl;
            check_plugin_dir(local_dir, "local", total_plugins, errors, warnings);
        }
    }

    // Summary
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Total plugins: " << total_plugins << std::endl;
    std::cout << "Errors: " << errors << std::endl;
    std::cout << "Warnings: " << warnings << std::endl;

    if (errors == 0 && warnings == 0) {
        std::cout << termcolor::green << "[OK] All plugins are healthy" << termcolor::reset << std::endl;
        return 0;
    } else if (errors == 0) {
        std::cout << termcolor::green << "[OK] No critical errors found" << termcolor::reset << std::endl;
        return 0;
    } else {
        std::cout << termcolor::red << "[!] Issues found - review above" << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace spark

#endif // SPARK_COMMANDS_CHECK_HPP