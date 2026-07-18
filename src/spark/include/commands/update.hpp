#ifndef SPARK_COMMANDS_UPDATE_HPP
#define SPARK_COMMANDS_UPDATE_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include "core/resolver.hpp"
#include "core/lockfile.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include "termcolor.hpp"

namespace spark {
namespace commands {

// Compare two semantic versions (returns true if v1 < v2)
// Moved ABOVE get_latest_github_version so it can be used for directory comparison
inline bool is_version_older(const std::string& v1, const std::string& v2) {
    if (v1 == v2) return false;
    if (v1 == "latest" || v1.empty()) return true;
    if (v2 == "latest" || v2.empty()) return false;
    
    std::vector<int> parts1, parts2;
    std::stringstream ss1(v1), ss2(v2);
    std::string token;
    
    while (std::getline(ss1, token, '.')) {
        parts1.push_back(std::stoi(token));
    }
    while (std::getline(ss2, token, '.')) {
        parts2.push_back(std::stoi(token));
    }
    
    size_t max_size = std::max(parts1.size(), parts2.size());
    parts1.resize(max_size, 0);
    parts2.resize(max_size, 0);
    
    for (size_t i = 0; i < max_size; i++) {
        if (parts1[i] < parts2[i]) return true;
        if (parts1[i] > parts2[i]) return false;
    }
    
    return false;
}

// Fetch the latest version tag from a GitHub repository
inline std::string get_latest_github_version(const std::string& repo_url) {
    // Parse owner/repo from URL
    // Expected: https://github.com/owner/repo
    std::string api_url = repo_url;
    
    // Extract owner/repo from GitHub URL
    size_t github_pos = api_url.find("github.com/");
    if (github_pos == std::string::npos) {
        return "";
    }
    
    std::string owner_repo = api_url.substr(github_pos + 11);
    // Remove trailing slash if present
    if (!owner_repo.empty() && owner_repo.back() == '/') {
        owner_repo.pop_back();
    }
    // Remove .git suffix if present
    if (owner_repo.size() > 4 && owner_repo.substr(owner_repo.size() - 4) == ".git") {
        owner_repo = owner_repo.substr(0, owner_repo.size() - 4);
    }
    
    std::cout << termcolor::cyan << "[*] Checking for updates at: " << api_url << termcolor::reset << std::endl;
    
    try {
        httplib::SSLClient cli("api.github.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(15);
        cli.enable_server_certificate_verification(false);
        
        // Add User-Agent header (required by GitHub API)
        httplib::Headers headers = {
            {"User-Agent", "Sapphire-Mine/2.0"},
            {"Accept", "application/vnd.github.v3+json"}
        };
        
        // 1. Try Releases
        std::string api_path = "/repos/" + owner_repo + "/releases/latest";
        auto res = cli.Get(api_path.c_str(), headers);
        
        if (res && res->status == 200) {
            try {
                nlohmann::json j = nlohmann::json::parse(res->body);
                if (j.contains("tag_name")) {
                    std::string tag = j["tag_name"].get<std::string>();
                    if (tag.size() > 1 && tag[0] == 'v') { tag = tag.substr(1); }
                    return tag;
                }
            } catch (...) {}
        } 
        
        // 2. Try Tags Fallback
        std::string tags_path = "/repos/" + owner_repo + "/tags";
        auto tags_res = cli.Get(tags_path.c_str(), headers);
        if (tags_res && tags_res->status == 200) {
            try {
                nlohmann::json tags = nlohmann::json::parse(tags_res->body);
                if (tags.is_array() && !tags.empty()) {
                    std::string tag = tags[0]["name"].get<std::string>();
                    if (tag.size() > 1 && tag[0] == 'v') { tag = tag.substr(1); }
                    return tag;
                }
            } catch (...) {}
        }

        // 3. Try versions/ Directory Check (Sapphire Native Structure Fallback)
        std::string contents_path = "/repos/" + owner_repo + "/contents/versions";
        auto contents_res = cli.Get(contents_path.c_str(), headers);
        if (contents_res && contents_res->status == 200) {
            try {
                nlohmann::json items = nlohmann::json::parse(contents_res->body);
                std::string highest_version = "";
                
                if (items.is_array()) {
                    for (const auto& item : items) {
                        if (item.contains("type") && item["type"] == "dir" && item.contains("name")) {
                            std::string dir_name = item["name"].get<std::string>();
                            std::string ver = dir_name;
                            
                            // Remove 'v' prefix for comparison
                            if (ver.size() > 1 && ver[0] == 'v') {
                                ver = ver.substr(1);
                            }
                            
                            if (highest_version.empty() || is_version_older(highest_version, ver)) {
                                highest_version = ver;
                            }
                        }
                    }
                }
                
                if (!highest_version.empty()) {
                    return highest_version;
                }
            } catch (const std::exception& e) {
                std::cerr << termcolor::yellow << "  [!] Failed to parse versions directory: " << e.what() << termcolor::reset << std::endl;
            }
        }
        
        // If all checks fail, output error
        if ((!res || res->status != 200) && 
            (!tags_res || tags_res->status != 200) && 
            (!contents_res || contents_res->status != 200)) {
             std::cerr << termcolor::yellow << "  [!] Could not find any releases, tags, or 'versions/' directory" << termcolor::reset << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << termcolor::yellow << "[!] Update check error: " << e.what() << termcolor::reset << std::endl;
    }
    
    return "";
}

// Update a specific plugin to the latest version
inline int cmd_update(const std::string& plugin_name = "", bool check_only = false) {
    // If no plugin name given, update all installed plugins
    std::vector<std::string> plugins_to_update;
    
    if (plugin_name.empty()) {
        // Collect all installed plugins (both local and global)
        std::cout << termcolor::cyan << "[*] Checking all installed plugins for updates..." << termcolor::reset << std::endl;
        
        // Check global plugins
        fs::path global_dir = get_plugin_dir();
        if (fs::exists(global_dir)) {
            for (const auto& entry : fs::directory_iterator(global_dir)) {
                if (!fs::is_directory(entry.path())) continue;
                std::string name = entry.path().filename().string();
                if (name == ".cache") continue;
                plugins_to_update.push_back(name);
            }
        }
        
        // Check local plugins
        if (is_sapphire_project()) {
            fs::path local_dir = get_local_plugin_dir();
            if (fs::exists(local_dir)) {
                for (const auto& entry : fs::directory_iterator(local_dir)) {
                    if (!fs::is_directory(entry.path())) continue;
                    std::string name = entry.path().filename().string();
                    // Avoid duplicates
                    if (std::find(plugins_to_update.begin(), plugins_to_update.end(), name) == plugins_to_update.end()) {
                        plugins_to_update.push_back(name);
                    }
                }
            }
        }
        
        if (plugins_to_update.empty()) {
            std::cout << termcolor::yellow << "No plugins installed to update" << termcolor::reset << std::endl;
            return 0;
        }
        
        std::cout << termcolor::yellow << "[*] Found " << plugins_to_update.size() << " plugin(s) to check" << termcolor::reset << std::endl;
    } else {
        // Check if plugin exists
        if (!is_plugin_installed_anywhere(plugin_name)) {
            std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' is not installed" << termcolor::reset << std::endl;
            return 1;
        }
        plugins_to_update.push_back(plugin_name);
    }
    
    int updated_count = 0;
    int up_to_date_count = 0;
    int failed_count = 0;
    
    for (const auto& name : plugins_to_update) {
        std::cout << termcolor::cyan << "\n[*] Checking: " << name << termcolor::reset << std::endl;
        
        // Get the plugin base directory (local or global)
        fs::path plugin_base = get_plugin_base_dir(name);
        
        // Read the current version from PLUGIN.txt
        fs::path plugin_txt_path = plugin_base / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_txt_path);
        
        if (!meta) {
            std::cerr << termcolor::yellow << "  [!] Cannot read PLUGIN.txt for " << name << ", skipping" << termcolor::reset << std::endl;
            failed_count++;
            continue;
        }
        
        std::string current_version = meta->version;
        std::cout << "[*] Current version: " << current_version << std::endl;
        
        // Query registry for the repository URL
        auto registry_entry = query_registry(name);
        if (!registry_entry) {
            std::cerr << termcolor::yellow << "  [!] Cannot find plugin in registry, skipping" << termcolor::reset << std::endl;
            failed_count++;
            continue;
        }
        
        // Get the latest version from GitHub
        std::string latest_version = get_latest_github_version(registry_entry->repository);
        
        if (latest_version.empty()) {
            std::cerr << termcolor::yellow << "  [!] Could not determine latest version from GitHub" << termcolor::reset << std::endl;
            failed_count++;
            continue;
        }
        
        std::cout << "[*] Latest version: " << latest_version << std::endl;
        
        // Compare versions
        if (current_version == latest_version) {
            std::cout << termcolor::green << "  [OK] Already up to date" << termcolor::reset << std::endl;
            up_to_date_count++;
            continue;
        }
        
        if (!is_version_older(current_version, latest_version)) {
            std::cout << termcolor::yellow << "  [*] Current version is newer than latest (dev/pre-release)" << termcolor::reset << std::endl;
            up_to_date_count++;
            continue;
        }
        
        if (check_only) {
            std::cout << termcolor::yellow << "  [*] Update available: v" << current_version << " -> v" << latest_version << " (run without --check-only to update)" << termcolor::reset << std::endl;
            updated_count++;
            continue;
        }
        
        // Ask for confirmation before updating
        std::cout << termcolor::yellow << "[?] Update " << name << " from v" << current_version << " to v" << latest_version << "? (Y/n): " << termcolor::reset;
        std::string response;
        std::getline(std::cin, response);
        response = trim(response);
        
        if (response == "n" || response == "N") {
            std::cout << termcolor::yellow << "  [*] Skipping update" << termcolor::reset << std::endl;
            continue;
        }
        
        std::cout << termcolor::cyan << "[*] Updating " << name << " to v" << latest_version << "..." << termcolor::reset << std::endl;
        
        // Check if we already have the latest version downloaded
        // First, download the main branch to get all versions
        if (!download_and_extract_plugin(name, registry_entry->repository, "latest")) {
            std::cerr << termcolor::red << "  [!] Failed to download update" << termcolor::reset << std::endl;
            failed_count++;
            continue;
        }
        
        // Check if the new version was actually installed
        // The download_and_extract_plugin updates the main branch which includes all versions
        // Now check if the specific version exists
        fs::path version_dir = get_plugin_version_path(name, latest_version);
        
        // We need to update the version in PLUGIN.txt to reflect the latest
        // But we need to write to the correct directory (local or global)
        fs::path target_base = get_plugin_base_dir(name);
        fs::path target_plugin_txt = target_base / "PLUGIN.txt";
        
        if (fs::exists(target_plugin_txt)) {
            // Read the current meta, update version, and write back
            auto updated_meta = parse_plugin_txt(target_plugin_txt);
            if (updated_meta) {
                updated_meta->version = latest_version;
                write_plugin_txt(target_plugin_txt, *updated_meta);
            }
        }
        
        // Generate lockfile for the new version
        DependencyResolver resolver(fs::current_path(), name);
        resolver.resolve_and_install(name, latest_version);
        resolver.write_lockfiles();
        
        std::cout << termcolor::green << "  [OK] Updated to v" << latest_version << termcolor::reset << std::endl;
        updated_count++;
    }
    
    // Summary
    std::cout << termcolor::cyan << "\n=== Update Summary ===" << termcolor::reset << std::endl;
    std::cout << termcolor::green << "Updated: " << updated_count << termcolor::reset << std::endl;
    std::cout << "Up to date: " << up_to_date_count << std::endl;
    if (failed_count > 0) {
        std::cout << termcolor::red << "Failed: " << failed_count << termcolor::reset << std::endl;
    }
    
    if (updated_count > 0) {
        std::cout << termcolor::green << "[OK] " << updated_count << " plugin(s) updated" << termcolor::reset << std::endl;
        return 0;
    } else if (failed_count == 0) {
        std::cout << "[*] All plugins are up to date" << std::endl;
        return 0;
    } else {
        return 1;
    }
}

} // namespace commands
} // namespace spark

#endif // SPARK_COMMANDS_UPDATE_HPP
