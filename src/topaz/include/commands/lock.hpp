#ifndef TOPAZ_COMMANDS_LOCK_HPP
#define TOPAZ_COMMANDS_LOCK_HPP

#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/lockfile.hpp"
#include "core/resolver.hpp"
#include "termcolor.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <iomanip>

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_lock_generate — Generate lock files for a plugin
//
// Usage:
//   topaz lock <name> [version]
// ---------------------------------------------------------------------------
inline int cmd_lock_generate(const std::string& plugin_name, const std::string& version = "latest") {
    if (plugin_name.empty()) {
        std::cerr << termcolor::red << "[!] Missing plugin name." << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz lock <name> [version]" << std::endl;
        return 1;
    }

    // Check if plugin is installed anywhere
    if (!is_plugin_installed_anywhere(plugin_name)) {
        std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' is not installed." << termcolor::reset << std::endl;
        std::cerr << "Install it first with: " << termcolor::green << "topaz install " << plugin_name << termcolor::reset << std::endl;
        return 1;
    }

    std::cout << termcolor::cyan << "[*] Generating lock file for: " << plugin_name << termcolor::reset;
    if (version != "latest") {
        std::cout << " (version: " << version << ")";
    }
    std::cout << std::endl;

    // Resolve the version to use
    std::string resolved_version = version;
    if (version == "latest") {
        // Get all available versions and use the latest
        auto versions = get_plugin_versions(plugin_name);
        if (versions.empty()) {
            std::cerr << termcolor::red << "[!] No versions found for plugin '" << plugin_name << "'" << termcolor::reset << std::endl;
            return 1;
        }
        // Use the last one (should be highest)
        resolved_version = versions.back();
        std::cout << termcolor::green << "[*] Using latest version: " << resolved_version << termcolor::reset << std::endl;
    }

    // Get the plugin base directory
    fs::path plugin_base = get_plugin_base_dir(plugin_name);

    // Check if the version exists
    fs::path version_dir = plugin_base / "versions" / ("v" + resolved_version);
    if (!fs::exists(version_dir)) {
        std::string clean_ver = resolved_version;
        if (clean_ver.size() > 0 && clean_ver[0] == 'v') clean_ver = clean_ver.substr(1);
        version_dir = plugin_base / "versions" / ("v" + clean_ver);
        if (!fs::exists(version_dir)) {
            std::cerr << termcolor::red << "[!] Version " << resolved_version << " not found for plugin '" << plugin_name << "'" << termcolor::reset << std::endl;
            return 1;
        }
        resolved_version = clean_ver;
    }

    // Run dependency resolution to generate lock files
    DependencyResolver resolver(fs::current_path(), plugin_name);
    resolver.resolve_and_install(plugin_name, resolved_version);
    resolver.write_lockfiles();

    // Check if lock file was created
    fs::path lockfile_path = version_dir / "topaz.lock";
    fs::path checksums_path = version_dir / "CHECKSUMS.txt";

    std::cout << std::endl;
    std::cout << termcolor::bold << "Lock File Summary:" << termcolor::reset << std::endl;
    std::cout << std::string(40, '-') << std::endl;

    if (fs::exists(lockfile_path)) {
        uintmax_t size = fs::file_size(lockfile_path);
        std::string size_str;
        if (size > 1024) {
            size_str = std::to_string(size / 1024) + " KB";
        } else {
            size_str = std::to_string(size) + " B";
        }
        std::cout << termcolor::green << "[OK] topaz.lock" << termcolor::reset
                  << " (" << size_str << ")" << std::endl;
        std::cout << "  Path: " << termcolor::cyan << lockfile_path.string() << termcolor::reset << std::endl;
    } else {
        std::cout << termcolor::red << "[!] topaz.lock was not created" << termcolor::reset << std::endl;
    }

    if (fs::exists(checksums_path)) {
        std::cout << termcolor::green << "[OK] CHECKSUMS.txt" << termcolor::reset << std::endl;
        std::cout << "  Path: " << termcolor::cyan << checksums_path.string() << termcolor::reset << std::endl;
    }

    // Read and display the lock file contents
    if (fs::exists(lockfile_path)) {
        std::cout << std::endl;
        LockFile lockfile(version_dir, plugin_name, resolved_version);
        if (lockfile.read()) {
            const auto& deps = lockfile.get_dependencies();
            std::cout << termcolor::bold << "Locked Dependencies:" << termcolor::reset << std::endl;
            for (const auto& dep : deps) {
                std::cout << "  - " << dep.name << " v" << dep.version;
                if (dep.checksum != "unknown" && dep.checksum != "latest") {
                    std::cout << "  [checksum: " << dep.checksum.substr(0, 16) << "...]";
                }
                if (!dep.dependencies.empty()) {
                    std::cout << "  (" << dep.dependencies.size() << " deps)";
                }
                std::cout << std::endl;
            }
        }
    }

    return 0;
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_LOCK_HPP