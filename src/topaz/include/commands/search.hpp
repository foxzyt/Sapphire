#ifndef TOPAZ_COMMANDS_SEARCH_HPP
#define TOPAZ_COMMANDS_SEARCH_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include "termcolor.hpp"
#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_search — Search for plugins in the central registry
//   Usage: topaz search <query>
// ---------------------------------------------------------------------------
inline int cmd_search(const std::string& query) {
    if (query.empty()) {
        std::cerr << termcolor::red << "[!] Missing search query" << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz search <query>" << std::endl;
        return 1;
    }

    std::cout << termcolor::cyan << "[*] Searching for plugins matching: " << query << termcolor::reset << std::endl;

    // GitHub repository for the plugin registry
    const char* REGISTRY_OWNER = "foxzyt";
    const char* REGISTRY_REPO = "Sapphire-Topaz";
    const char* REGISTRY_BRANCH = "main";
    const char* REGISTRY_FILE = "registry.json";

    try {
        httplib::Client cli("api.github.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(15);
        cli.set_read_timeout(30);


        std::string url_path = std::string("/repos/") + REGISTRY_OWNER
                              + "/" + REGISTRY_REPO
                              + "/contents";

        httplib::Headers headers = {
            {"User-Agent", "Sapphire-Mine/2.2"},
            {"Accept", "application/vnd.github.v3+json"}
        };
        auto res = cli.Get(url_path.c_str(), headers);

        if (!res) {
            std::cerr << termcolor::red << "[!] Failed to connect to registry API" << termcolor::reset << std::endl;
            return 1;
        }

        if (res->status == 404) {
            std::cerr << termcolor::yellow << "[!] Registry repository not found" << termcolor::reset << std::endl;
            std::cout << "Registry URL: https://github.com/" << REGISTRY_OWNER << "/" << REGISTRY_REPO << std::endl;
            return 1;
        }

        if (res->status != 200) {
            std::cerr << termcolor::red << "[!] HTTP " << res->status << " fetching registry contents" << termcolor::reset << std::endl;
            return 1;
        }

        nlohmann::json contents = nlohmann::json::parse(res->body);

        if (!contents.is_array()) {
            std::cerr << termcolor::red << "[!] Invalid registry format" << termcolor::reset << std::endl;
            return 1;
        }

        // Convert query to lowercase for case-insensitive search
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

        std::vector<std::string> matching_plugins;
        for (const auto& item : contents) {
            if (item.contains("name") && item.contains("type") && item["type"] == "file") {
                std::string file_name = item["name"].get<std::string>();
                if (file_name.size() > 5 && file_name.substr(file_name.size() - 5) == ".json") {
                    std::string plugin_name = file_name.substr(0, file_name.size() - 5);
                    if (plugin_name == "registry" || plugin_name == "package") continue;
                    
                    std::string plugin_name_lower = plugin_name;
                    std::transform(plugin_name_lower.begin(), plugin_name_lower.end(), plugin_name_lower.begin(), ::tolower);
                    if (plugin_name_lower.find(query_lower) != std::string::npos) {
                        matching_plugins.push_back(plugin_name);
                    }
                }
            }
        }

        if (matching_plugins.empty()) {
            std::cout << termcolor::yellow << "[*] No plugins found matching '" << query << "'" << termcolor::reset << std::endl;
            return 0;
        }

        std::cout << termcolor::green << "[OK] Found " << matching_plugins.size() << " plugin(s)" << termcolor::reset << std::endl;
        std::cout << std::endl;

        for (const auto& plugin_name : matching_plugins) {
            // Optional: fetch specific JSON for each to get author/desc, but to avoid rate limits, we just show the name.
            std::cout << termcolor::bold << termcolor::cyan << plugin_name << termcolor::reset << std::endl;
        }



        std::cout << "Install with: " << termcolor::green << "topaz install <name>" << termcolor::reset << std::endl;

    } catch (const nlohmann::json::exception& e) {
        std::cerr << termcolor::red << "[!] JSON parse error: " << e.what() << termcolor::reset << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << termcolor::red << "[!] Error: " << e.what() << termcolor::reset << std::endl;
        return 1;
    }

    return 0;
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_SEARCH_HPP
