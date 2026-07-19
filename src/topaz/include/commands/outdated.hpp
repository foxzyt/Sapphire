#ifndef TOPAZ_COMMANDS_OUTDATED_HPP
#define TOPAZ_COMMANDS_OUTDATED_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include "core/semver.hpp"
#include "termcolor.hpp"
#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_outdated — List plugins that have newer versions available
//
// Usage:
//   topaz outdated                    — List all outdated plugins
//   topaz outdated <name>             — Check a specific plugin
// ---------------------------------------------------------------------------
inline int cmd_outdated(const std::string& plugin_name = "") {
    std::vector<std::string> plugins_to_check;

    if (plugin_name.empty()) {
        // Collect all installed plugins (global + local)
        std::cout << termcolor::cyan << "[*] Checking all installed plugins for updates..."
                  << termcolor::reset << std::endl;

        // Global plugins
        fs::path global_dir = get_plugin_dir();
        if (fs::exists(global_dir)) {
            for (const auto& entry : fs::directory_iterator(global_dir)) {
                if (!fs::is_directory(entry.path())) continue;
                std::string name = entry.path().filename().string();
                if (name == ".cache") continue;
                plugins_to_check.push_back(name);
            }
        }

        // Local plugins (avoid duplicates)
        if (is_sapphire_project()) {
            fs::path local_dir = get_local_plugin_dir();
            if (fs::exists(local_dir)) {
                for (const auto& entry : fs::directory_iterator(local_dir)) {
                    if (!fs::is_directory(entry.path())) continue;
                    std::string name = entry.path().filename().string();
                    if (name == ".cache") continue;
                    if (std::find(plugins_to_check.begin(), plugins_to_check.end(), name) == plugins_to_check.end()) {
                        plugins_to_check.push_back(name);
                    }
                }
            }
        }

        if (plugins_to_check.empty()) {
            std::cout << termcolor::yellow << "[*] No plugins installed." << termcolor::reset << std::endl;
            return 0;
        }
    } else {
        if (!is_plugin_installed_anywhere(plugin_name)) {
            std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' is not installed." << termcolor::reset << std::endl;
            return 1;
        }
        plugins_to_check.push_back(plugin_name);
    }

    // Display header
    std::cout << std::endl;
    std::cout << std::left
              << std::setw(30) << "Plugin"
              << std::setw(18) << "Installed"
              << std::setw(18) << "Latest"
              << "Status"
              << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    int outdated_count = 0;
    int up_to_date_count = 0;
    int error_count = 0;

    for (const auto& name : plugins_to_check) {
        // Read current version
        fs::path plugin_base = get_plugin_base_dir(name);
        fs::path plugin_txt_path = plugin_base / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_txt_path);

        if (!meta) {
            std::cout << std::left << std::setw(30) << name
                      << std::setw(18) << "(unknown)"
                      << std::setw(18) << "?"
                      << termcolor::yellow << " [can't read PLUGIN.txt]" << termcolor::reset
                      << std::endl;
            error_count++;
            continue;
        }

        std::string current_version = meta->version;

        // Query registry for repository URL
        auto registry_entry = query_registry(name);
        if (!registry_entry) {
            std::cout << std::left << std::setw(30) << name
                      << std::setw(18) << current_version
                      << std::setw(18) << "?"
                      << termcolor::yellow << " [not in registry]" << termcolor::reset
                      << std::endl;
            error_count++;
            continue;
        }

        // Fetch latest version from GitHub
        std::string latest_version = fetch_latest_version_from_repo(registry_entry->repository);

        if (latest_version.empty()) {
            std::cout << std::left << std::setw(30) << name
                      << std::setw(18) << current_version
                      << std::setw(18) << "?"
                      << termcolor::yellow << " [can't fetch remote]" << termcolor::reset
                      << std::endl;
            error_count++;
            continue;
        }

        // Remove 'v' prefix from both for comparison
        std::string clean_current = current_version;
        if (clean_current.size() > 0 && clean_current[0] == 'v') clean_current = clean_current.substr(1);
        std::string clean_latest = latest_version;
        if (clean_latest.size() > 0 && clean_latest[0] == 'v') clean_latest = clean_latest.substr(1);

        if (clean_current == clean_latest) {
            std::cout << std::left << std::setw(30) << name
                      << std::setw(18) << current_version
                      << std::setw(18) << latest_version
                      << termcolor::green << " [up-to-date]" << termcolor::reset
                      << std::endl;
            up_to_date_count++;
        } else {
            std::cout << std::left << std::setw(30) << name
                      << std::setw(18) << current_version
                      << std::setw(18) << latest_version
                      << termcolor::red << " [OUTDATED]" << termcolor::reset
                      << std::endl;
            outdated_count++;
        }
    }

    // Summary
    std::cout << std::string(80, '-') << std::endl;
    std::cout << termcolor::green << "Up-to-date: " << up_to_date_count << termcolor::reset;
    if (outdated_count > 0) {
        std::cout << "  " << termcolor::red << "Outdated: " << outdated_count << termcolor::reset;
    }
    if (error_count > 0) {
        std::cout << "  " << termcolor::yellow << "Errors: " << error_count << termcolor::reset;
    }
    std::cout << std::endl;

    if (outdated_count > 0) {
        std::cout << std::endl;
        std::cout << "Run " << termcolor::green << "topaz update <name>" << termcolor::reset
                  << " to update a specific plugin." << std::endl;
    }

    return (outdated_count == 0) ? 0 : 1;
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_OUTDATED_HPP