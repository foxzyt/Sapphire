#ifndef SPARK_COMMANDS_SEARCH_HPP
#define SPARK_COMMANDS_SEARCH_HPP

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

namespace spark {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_search — Search for plugins in the central registry
//   Usage: spark search <query>
// ---------------------------------------------------------------------------
inline int cmd_search(const std::string& query) {
    if (query.empty()) {
        std::cerr << termcolor::red << "[!] Missing search query" << termcolor::reset << std::endl;
        std::cerr << "Usage: spark search <query>" << std::endl;
        return 1;
    }

    std::cout << termcolor::cyan << "[*] Searching for plugins matching: " << query << termcolor::reset << std::endl;

    // GitHub repository for the plugin registry
    const char* REGISTRY_OWNER = "foxzyt";
    const char* REGISTRY_REPO = "sapphire-spark";
    const char* REGISTRY_BRANCH = "main";
    const char* REGISTRY_FILE = "registry.json";

    try {
        httplib::SSLClient cli("raw.githubusercontent.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(15);
        cli.set_read_timeout(30);
        cli.enable_server_certificate_verification(false);

        std::string url_path = std::string("/") + REGISTRY_OWNER
                              + "/" + REGISTRY_REPO
                              + "/" + REGISTRY_BRANCH
                              + "/" + REGISTRY_FILE;

        auto res = cli.Get(url_path.c_str());

        if (!res) {
            std::cerr << termcolor::red << "[!] Failed to connect to registry" << termcolor::reset << std::endl;
            return 1;
        }

        if (res->status == 404) {
            std::cerr << termcolor::yellow << "[!] Registry file not found" << termcolor::reset << std::endl;
            std::cout << "Registry URL: https://github.com/" << REGISTRY_OWNER << "/" << REGISTRY_REPO << std::endl;
            return 1;
        }

        if (res->status != 200) {
            std::cerr << termcolor::red << "[!] HTTP " << res->status << " fetching registry" << termcolor::reset << std::endl;
            return 1;
        }

        nlohmann::json registry = nlohmann::json::parse(res->body);

        if (!registry.is_object() || !registry.contains("plugins")) {
            std::cerr << termcolor::red << "[!] Invalid registry format" << termcolor::reset << std::endl;
            return 1;
        }

        nlohmann::json plugins = registry["plugins"];
        if (!plugins.is_array()) {
            std::cerr << termcolor::red << "[!] Invalid plugins format in registry" << termcolor::reset << std::endl;
            return 1;
        }

        // Convert query to lowercase for case-insensitive search
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

        std::vector<nlohmann::json> results;

        for (const auto& plugin : plugins) {
            if (!plugin.contains("name")) continue;

            std::string name = plugin["name"].get<std::string>();
            std::string name_lower = name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

            // Search in name and description
            bool matches = (name_lower.find(query_lower) != std::string::npos);

            if (!matches && plugin.contains("description")) {
                std::string desc = plugin["description"].get<std::string>();
                std::string desc_lower = desc;
                std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(), ::tolower);
                matches = (desc_lower.find(query_lower) != std::string::npos);
            }

            if (matches) {
                results.push_back(plugin);
            }
        }

        if (results.empty()) {
            std::cout << termcolor::yellow << "[*] No plugins found matching '" << query << "'" << termcolor::reset << std::endl;
            return 0;
        }

        std::cout << termcolor::green << "[OK] Found " << results.size() << " plugin(s)" << termcolor::reset << std::endl;
        std::cout << std::endl;

        for (const auto& plugin : results) {
            std::string name = plugin.value("name", "Unknown");
            std::string author = plugin.value("author", "Unknown");
            std::string description = plugin.value("description", "");
            std::string version = plugin.value("latest_version", "N/A");
            std::string url = plugin.value("url", "");

            std::cout << termcolor::bold << termcolor::cyan << name << termcolor::reset << std::endl;
            std::cout << "  " << termcolor::yellow << "Author:" << termcolor::reset << " " << author << std::endl;
            std::cout << "  " << termcolor::yellow << "Version:" << termcolor::reset << " " << version << std::endl;

            if (!description.empty()) {
                std::cout << "  " << termcolor::yellow << "Description:" << termcolor::reset << " " << description << std::endl;
            }

            if (!url.empty()) {
                std::cout << "  " << termcolor::yellow << "URL:" << termcolor::reset << " " << url << std::endl;
            }

            std::cout << std::endl;
        }

        std::cout << "Install with: " << termcolor::green << "spark install <name>" << termcolor::reset << std::endl;

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
} // namespace spark

#endif // SPARK_COMMANDS_SEARCH_HPP
