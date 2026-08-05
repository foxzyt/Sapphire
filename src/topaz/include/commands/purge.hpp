#ifndef TOPAZ_COMMANDS_PURGE_HPP
#define TOPAZ_COMMANDS_PURGE_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "termcolor.hpp"
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include "commands/uninstall.hpp"

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_purge — Remove a specific version or all versions of a plugin
//
// Usage:
//   topaz purge <name>                     — Remove all versions from both scopes
//   topaz purge <name> --local             — Remove all versions from local only
//   topaz purge <name> --global            — Remove all versions from global only
//   topaz purge <name> <version>           — Remove a specific version from both
//   topaz purge <name> <version> --local   — Remove a specific version from local
//   topaz purge <name> <version> --global  — Remove a specific version from global
// ---------------------------------------------------------------------------
inline int cmd_purge(const std::string& plugin_name, const std::string& version = "",
                     bool local_only = false, bool global_only = false) {
    if (plugin_name.empty()) {
        std::cerr << termcolor::red << "[!] Missing plugin name." << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz purge <name> [version] [--local] [--global]" << std::endl;
        return 1;
    }

    bool has_local = is_plugin_installed_local(plugin_name);
    bool has_global = is_plugin_installed(plugin_name);

    if (!has_local && !has_global) {
        std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' is not installed anywhere." << termcolor::reset << std::endl;
        return 1;
    }

    std::cout << termcolor::bold << "Purge: " << plugin_name << termcolor::reset << std::endl;
    if (!version.empty()) {
        std::cout << "  Version: " << version << std::endl;
    }
    std::cout << std::string(40, '-') << std::endl;

    // Check if plugin is required by others
    if (is_plugin_required_by_others(plugin_name)) {
        std::cerr << termcolor::yellow << "[!] Warning: Plugin '" << plugin_name << "' is required by other plugins" << termcolor::reset << std::endl;
        std::cerr << termcolor::yellow << "    Purging it may break dependent plugins and cause silent downgrades" << termcolor::reset << std::endl;
        std::cout << termcolor::bold << termcolor::red << "Are you absolutely sure you want to purge it? (y/N): " << termcolor::reset;
        std::string response;
        std::getline(std::cin, response);
        response = trim(response);
        if (response != "y" && response != "Y") {
            if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Purge cancelled." << termcolor::reset << std::endl;
            return 0;
        }
    }

    int removed_count = 0;

    // Helper lambda to remove a specific version from a directory
    auto remove_version = [&](const fs::path& base_dir, const std::string& scope_name) {
        // Try with 'v' prefix
        fs::path ver_dir_v = base_dir / plugin_name / "versions" / ("v" + version);
        fs::path ver_dir_clean = base_dir / plugin_name / "versions" / version;

        fs::path target_dir;
        if (fs::exists(ver_dir_v)) {
            target_dir = ver_dir_v;
        } else if (fs::exists(ver_dir_clean)) {
            target_dir = ver_dir_clean;
        } else {
            return false;
        }

        try {
            fs::remove_all(target_dir);
            std::cout << termcolor::green << "  [OK] Removed v" << version << " from " << scope_name << termcolor::reset << std::endl;

            // Check if the versions directory is now empty, and if so, also remove the lock/checksum files
            fs::path versions_dir = base_dir / plugin_name / "versions";
            bool has_remaining = false;
            if (fs::exists(versions_dir)) {
                for (const auto& entry : fs::directory_iterator(versions_dir)) {
                    if (fs::is_directory(entry.path())) {
                        has_remaining = true;
                        break;
                    }
                }
            }

            // If no versions remain, clean up lock files too
            if (!has_remaining) {
                fs::path lockfile = base_dir / plugin_name / "topaz.lock";
                if (fs::exists(lockfile)) {
                    fs::remove(lockfile);
                    std::cout << "  [i] Removed stale lockfile from " << scope_name << std::endl;
                }
                fs::path checksums = base_dir / plugin_name / "CHECKSUMS.txt";
                if (fs::exists(checksums)) {
                    fs::remove(checksums);
                }
            }

            return true;
        } catch (const std::exception& e) {
            std::cerr << termcolor::red << "  [!] Failed to remove: " << e.what() << termcolor::reset << std::endl;
            return false;
        }
    };

    // Helper lambda to remove all versions (entire plugin) from a directory
    auto remove_all = [&](const fs::path& base_dir, const std::string& scope_name) {
        fs::path plugin_path = base_dir / plugin_name;
        if (!fs::exists(plugin_path)) return false;

        try {
            // Show what versions are being removed
            fs::path versions_dir = plugin_path / "versions";
            if (fs::exists(versions_dir)) {
                std::cout << "  Removing versions: ";
                bool first = true;
                for (const auto& entry : fs::directory_iterator(versions_dir)) {
                    if (fs::is_directory(entry.path())) {
                        if (!first) std::cout << ", ";
                        std::cout << entry.path().filename().string();
                        first = false;
                    }
                }
                if (first) std::cout << "(none)";
                std::cout << std::endl;
            }

            fs::remove_all(plugin_path);
            std::cout << termcolor::green << "  [OK] Purged all versions from " << scope_name << termcolor::reset << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << termcolor::red << "  [!] Failed to purge: " << e.what() << termcolor::reset << std::endl;
            return false;
        }
    };

    // Determine which scopes to operate on
    bool purge_local = false;
    bool purge_global = false;

    if (local_only) {
        purge_local = has_local;
    } else if (global_only) {
        purge_global = has_global;
    } else {
        // Both scopes
        purge_local = has_local;
        purge_global = has_global;
    }

    // Ask for confirmation
    std::cout << "Proceed? (Y/n): ";
    std::string response;
    std::getline(std::cin, response);
    response = trim(response);
    if (response == "n" || response == "N") {
        if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Purge cancelled." << termcolor::reset << std::endl;
        return 0;
    }

    // Execute purge
    if (purge_local) {
        if (!version.empty()) {
            if (remove_version(get_local_plugin_dir(), "local")) removed_count++;
        } else {
            if (remove_all(get_local_plugin_dir(), "local")) removed_count++;
        }
    }

    if (purge_global) {
        if (!version.empty()) {
            if (remove_version(get_plugin_dir(), "global")) removed_count++;
        } else {
            if (remove_all(get_plugin_dir(), "global")) removed_count++;
        }
    }

    if (removed_count > 0) {
        std::cout << termcolor::green << "[OK] Purge completed." << termcolor::reset << std::endl;
        return 0;
    } else {
        if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Nothing was removed." << termcolor::reset << std::endl;
        return 0;
    }
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_PURGE_HPP