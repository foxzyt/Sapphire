#ifndef TOPAZ_COMMANDS_CACHE_CLEAN_HPP
#define TOPAZ_COMMANDS_CACHE_CLEAN_HPP

#include "core/fs_utils.hpp"
#include "termcolor.hpp"
#include <iostream>
#include <filesystem>

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_cache_clean — Clean the Topaz cache directory
//
// Usage:
//   topaz cache clean
// ---------------------------------------------------------------------------
inline int cmd_cache_clean(bool force = false) {
    fs::path cache_dir = get_cache_dir();

    if (!fs::exists(cache_dir)) {
        if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Cache directory does not exist." << termcolor::reset << std::endl;
        return 0;
    }

    // Count cache size before cleaning
    uintmax_t total_size = 0;
    int file_count = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(cache_dir)) {
            if (fs::is_regular_file(entry.path())) {
                total_size += fs::file_size(entry.path());
                file_count++;
            }
        }
    } catch (...) {}

    if (topaz::g_verbose) std::cout << termcolor::cyan << "[*] Cleaning cache..." << termcolor::reset << std::endl;
    std::cout << "  Files to remove: " << file_count << std::endl;

    std::string size_str;
    if (total_size > 1024 * 1024) {
        size_str = std::to_string(total_size / (1024 * 1024)) + " MB";
    } else if (total_size > 1024) {
        size_str = std::to_string(total_size / 1024) + " KB";
    } else {
        size_str = std::to_string(total_size) + " B";
    }
    std::cout << "  Space to free: " << size_str << std::endl;

    if (!force) {
        // Ask for confirmation
        std::cout << "Proceed with cache cleanup? (Y/n): ";
        std::string response;
        std::getline(std::cin, response);
        response = trim(response);

        if (response == "n" || response == "N") {
            if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Cache cleanup cancelled." << termcolor::reset << std::endl;
            return 0;
        }
    }

    // Perform cleanup
    try {
        cleanup_cache();
        std::cout << termcolor::green << "[OK] Cache cleaned successfully." << termcolor::reset << std::endl;
        std::cout << "  Freed: " << size_str << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << termcolor::red << "[!] Failed to clean cache: " << e.what() << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_CACHE_CLEAN_HPP