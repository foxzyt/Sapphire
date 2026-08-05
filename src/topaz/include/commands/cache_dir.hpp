#ifndef TOPAZ_COMMANDS_CACHE_DIR_HPP
#define TOPAZ_COMMANDS_CACHE_DIR_HPP

#include "core/fs_utils.hpp"
#include "termcolor.hpp"
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_cache_dir — Show the cache directory path and its contents
//
// Usage:
//   topaz cache dir
// ---------------------------------------------------------------------------
inline int cmd_cache_dir() {
    fs::path cache_dir = get_cache_dir();

    std::cout << termcolor::bold << "Topaz Cache Directory" << termcolor::reset << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << "Path: " << termcolor::cyan << cache_dir.string() << termcolor::reset << std::endl;
    std::cout << std::endl;

    if (!fs::exists(cache_dir)) {
        if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Cache directory is empty or does not exist." << termcolor::reset << std::endl;
        return 0;
    }

    // List contents and calculate total size
    int file_count = 0;
    uintmax_t total_size = 0;

    std::cout << termcolor::bold << "Contents:" << termcolor::reset << std::endl;

    for (const auto& entry : fs::directory_iterator(cache_dir)) {
        std::string filename = entry.path().filename().string();
        uintmax_t size = 0;

        if (fs::is_regular_file(entry.path())) {
            size = fs::file_size(entry.path());
            total_size += size;
            file_count++;

            std::string size_str;
            if (size > 1024 * 1024) {
                size_str = std::to_string(size / (1024 * 1024)) + " MB";
            } else if (size > 1024) {
                size_str = std::to_string(size / 1024) + " KB";
            } else {
                size_str = std::to_string(size) + " B";
            }

            std::cout << "  " << termcolor::green << "[FILE] " << termcolor::reset
                      << std::left << std::setw(50) << filename
                      << size_str << std::endl;
        } else if (fs::is_directory(entry.path())) {
            // Count directory size recursively
            uintmax_t dir_size = 0;
            int dir_files = 0;
            for (const auto& rec : fs::recursive_directory_iterator(entry.path())) {
                if (fs::is_regular_file(rec.path())) {
                    dir_size += fs::file_size(rec.path());
                    dir_files++;
                }
            }
            total_size += dir_size;
            file_count += dir_files;

            std::string size_str;
            if (dir_size > 1024 * 1024) {
                size_str = std::to_string(dir_size / (1024 * 1024)) + " MB";
            } else if (dir_size > 1024) {
                size_str = std::to_string(dir_size / 1024) + " KB";
            } else {
                size_str = std::to_string(dir_size) + " B";
            }

            std::cout << "  " << termcolor::yellow << "[DIR]  " << termcolor::reset
                      << std::left << std::setw(50) << filename
                      << size_str << " (" << dir_files << " files)" << std::endl;
        }
    }

    // Summary
    std::cout << std::string(50, '-') << std::endl;
    std::string total_size_str;
    if (total_size > 1024 * 1024) {
        total_size_str = std::to_string(total_size / (1024 * 1024)) + " MB";
    } else if (total_size > 1024) {
        total_size_str = std::to_string(total_size / 1024) + " KB";
    } else {
        total_size_str = std::to_string(total_size) + " B";
    }

    std::cout << "Total: " << file_count << " file(s), " << total_size_str << std::endl;
    std::cout << std::endl;
    std::cout << "Run " << termcolor::green << "topaz cache clean" << termcolor::reset
              << " to clear the cache." << std::endl;

    return 0;
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_CACHE_DIR_HPP