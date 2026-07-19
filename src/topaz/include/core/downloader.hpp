#ifndef TOPAZ_DOWNLOADER_HPP
#define TOPAZ_DOWNLOADER_HPP

#include "types.hpp"
#include "fs_utils.hpp"
#include "parser.hpp"
#include "semver.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include "termcolor.hpp"

#include "httplib.h"
#include "json.hpp"
#include "miniz.h"

namespace topaz {

inline bool g_offline = false;
inline std::string g_registry_url = "https://raw.githubusercontent.com/foxzyt/Sapphire-Topaz/main/";

// Helper: Parse any URL into host and path (supports http and https)
inline bool parse_url(const std::string& url, std::string& host, std::string& path) {
    size_t protocol_pos = url.find("://");
    if (protocol_pos == std::string::npos) return false;
    size_t host_end = url.find('/', protocol_pos + 3);
    if (host_end == std::string::npos) {
        host = url.substr(protocol_pos + 3);
        path = "/";
        return true;
    }
    host = url.substr(protocol_pos + 3, host_end - protocol_pos - 3);
    path = url.substr(host_end);
    return true;
}

// Structure to hold registry information
struct RegistryEntry {
    std::string name;
    std::string repository;
    std::string checksum;
};

// Parse JSON from registry
inline std::optional<RegistryEntry> parse_registry_json(const std::string& json_content) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_content);
        
        RegistryEntry entry;
        if (j.contains("name")) {
            entry.name = j["name"].get<std::string>();
        }
        if (j.contains("repository")) {
            entry.repository = j["repository"].get<std::string>();
        }
        if (j.contains("sha256")) {
            entry.checksum = j["sha256"].get<std::string>();
        } else if (j.contains("checksum")) {
            entry.checksum = j["checksum"].get<std::string>();
        }
        
        if (!entry.name.empty() && !entry.repository.empty()) {
            return entry;
        }
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[!] JSON parsing error: " << e.what() << std::endl;
    }
    
    return std::nullopt;
}

// Query the central registry for a plugin
inline std::optional<RegistryEntry> query_registry(const std::string& plugin_name) {
    if (g_offline) {
        std::cerr << "[!] Offline mode: Cannot query remote registry for '" << plugin_name << "'" << std::endl;
        return std::nullopt;
    }
    try {
        std::string full_url = g_registry_url;
        if (full_url.empty()) return std::nullopt;
        if (full_url.back() != '/') full_url += "/";
        full_url += plugin_name + ".json";
        
        std::string host, path;
        if (!parse_url(full_url, host, path)) {
            std::cerr << "[!] Invalid registry URL: " << full_url << std::endl;
            return std::nullopt;
        }
        
        httplib::SSLClient cli(host);
        cli.set_follow_location(true);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(30);
        cli.enable_server_certificate_verification(false);
        
        auto res = cli.Get(path.c_str());
        
        if (res && res->status == 200) {
            return parse_registry_json(res->body);
        } else if (res) {
            if (res->status == 404) {
                std::cerr << "[!] Plugin '" << plugin_name << "' not found in central registry" << std::endl;
                std::cerr << "[!] Available plugins can be found at: https://github.com/foxzyt/Sapphire-Topaz" << std::endl;
            } else {
                std::cerr << "[!] HTTP error: " << res->status << " while querying registry" << std::endl;
            }
        } else {
            std::cerr << "[!] Connection failed to registry at: " << host << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] Registry query error: " << e.what() << std::endl;
    }
    
    return std::nullopt;
}

// Download a file from URL to local path
inline bool download_file(const std::string& url, const fs::path& output_path) {
    if (g_offline) {
        std::cerr << "[!] Offline mode: Blocked download request for " << url << std::endl;
        return false;
    }
    try {
        std::string host, path;
        if (!parse_url(url, host, path)) {
            std::cerr << "[!] Invalid URL format: " << url << std::endl;
            return false;
        }
        
        std::cout << "[*] Connecting to host: " << host << std::endl;
        std::cout << "[*] Requesting path: " << path << std::endl;
        
        httplib::SSLClient cli(host);
        cli.set_follow_location(true);
        cli.set_connection_timeout(30);
        cli.set_read_timeout(60);
        cli.enable_server_certificate_verification(false);
        
        auto res = cli.Get(path.c_str());
        
        if (res && res->status == 200) {
            // Create parent directory if it doesn't exist
            fs::create_directories(output_path.parent_path());
            
            std::ofstream out(output_path, std::ios::binary);
            if (!out.is_open()) {
                std::cerr << "[!] Failed to open output file: " << output_path << std::endl;
                return false;
            }
            
            out.write(res->body.data(), res->body.size());
            out.close();
            
            std::cout << "[*] Downloaded " << res->body.size() << " bytes" << std::endl;
            return true;
        } else if (res) {
            std::cerr << "[!] HTTP error: " << res->status << " (" << res->reason << ")" << std::endl;
        } else {
            std::cerr << "[!] Connection failed to " << host << std::endl;
            std::cerr << "[!] Check your internet connection" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] Download error: " << e.what() << std::endl;
    }
    
    return false;
}

// Convert GitHub repository URL to ZIP download URL
inline std::string github_to_zip_url(const std::string& repo_url, const std::string& version = "latest") {
    // Expected format: https://github.com/AutorOriginal/economy-sapphire
    // Convert to: https://github.com/AutorOriginal/economy-sapphire/archive/refs/heads/main.zip
    // Or for specific version: https://github.com/AutorOriginal/economy-sapphire/archive/refs/tags/v1.0.0.zip
    
    std::string result = repo_url;
    if (result.find("github.com/") != std::string::npos) {
        // Remove trailing slash if present
        if (result.back() == '/') {
            result.pop_back();
        }
        
        if (version == "latest") {
            result += "/archive/refs/heads/main.zip";
        } else {
            result += "/archive/refs/tags/" + version + ".zip";
        }
    }
    return result;
}

// Extract ZIP file to directory
inline bool extract_zip(const fs::path& zip_path, const fs::path& dest_dir) {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    
    if (!mz_zip_reader_init_file(&zip, zip_path.string().c_str(), 0)) {
        std::cerr << "[!] Failed to open ZIP file: " << zip_path << std::endl;
        return false;
    }
    
    // Create destination directory
    fs::create_directories(dest_dir);
    
    bool success = true;
    int file_count = mz_zip_reader_get_num_files(&zip);
    
    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) {
            continue;
        }
        
        // Skip if filename is empty
        if (file_stat.m_filename == nullptr || std::strlen(file_stat.m_filename) == 0) {
            continue;
        }
        
        fs::path out_path = dest_dir / file_stat.m_filename;
        
        if (file_stat.m_is_directory) {
            // Create directory
            fs::create_directories(out_path);
        } else {
            // Create parent directory if needed
            fs::create_directories(out_path.parent_path());
            
            // Extract file
            if (!mz_zip_reader_extract_file_to_file(&zip, file_stat.m_filename, out_path.string().c_str(), 0)) {
                std::cerr << "[!] Failed to extract: " << file_stat.m_filename << std::endl;
                success = false;
            }
        }
    }
    
    mz_zip_reader_end(&zip);
    return success;
}

// Download and extract a plugin from its repository
inline bool download_and_extract_plugin(const std::string& plugin_name, const std::string& repo_url, const std::string& version = "latest", const std::string& expected_checksum = "") {
    try {
        // Convert repo URL to ZIP URL
        std::string zip_url = github_to_zip_url(repo_url, version);
        std::cout << "[*] Downloading from: " << zip_url << std::endl;
        
        // Create a unique cache key for this download
        std::string cache_key = plugin_name + "_" + version + ".zip";
        fs::path zip_path = get_cache_dir() / cache_key;
        
        // Check if already cached
        if (fs::exists(zip_path)) {
            std::cout << "[*] Using cached download: " << cache_key << std::endl;
        } else {
            if (g_offline) {
                std::cerr << "[!] Offline mode error: Cache miss for " << plugin_name << " " << version << " (download blocked)" << std::endl;
                return false;
            }
            // Download ZIP to cache
            if (!download_file(zip_url, zip_path)) {
                std::cerr << "[!] Failed to download ZIP file" << std::endl;
                return false;
            }
        }
        
        if (!expected_checksum.empty()) {
            std::cout << "[*] Verifying checksum..." << std::endl;
            std::ifstream file(zip_path, std::ios::binary);
            if (file.is_open()) {
                EVP_MD_CTX* ctx = EVP_MD_CTX_new();
                EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
                
                char buffer[8192];
                while (file.read(buffer, sizeof(buffer))) {
                    EVP_DigestUpdate(ctx, buffer, file.gcount());
                }
                EVP_DigestUpdate(ctx, buffer, file.gcount());
                
                unsigned char hash[EVP_MAX_MD_SIZE];
                unsigned int hash_len;
                EVP_DigestFinal_ex(ctx, hash, &hash_len);
                EVP_MD_CTX_free(ctx);
                
                std::stringstream ss;
                for (unsigned int i = 0; i < hash_len; i++) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
                }
                
                std::string actual_checksum = ss.str();
                if (actual_checksum != expected_checksum) {
                    std::cerr << termcolor::red << "[!] SECURITY WARNING: Checksum mismatch for downloaded file!" << termcolor::reset << std::endl;
                    std::cerr << "Expected: " << expected_checksum << std::endl;
                    std::cerr << "Got:      " << actual_checksum << std::endl;
                    // According to user request, log warning but proceed or reject? The user said "emitindo apenas um alerta caso o pacote nao tenha assinatura... ou se prefere que bloqueie". The user approved the implementation plan. I will block if it HAS a checksum but it MISMATCHES.
                    return false;
                } else {
                    std::cout << termcolor::green << "[+] Checksum verified." << termcolor::reset << std::endl;
                }
            }
        } else {
            std::cout << termcolor::yellow << "[!] WARNING: No checksum provided by registry. Cannot verify signature." << termcolor::reset << std::endl;
        }
        
        // Extract to temporary directory
        fs::path extract_dir = get_cache_dir() / ("extracted_" + plugin_name + "_" + version);
        fs::create_directories(extract_dir);
        
        if (!extract_zip(zip_path, extract_dir)) {
            std::cerr << "[!] Failed to extract ZIP file" << std::endl;
            return false;
        }
        
        // GitHub ZIPs create a root directory (e.g., sapphire-grad-main or sapphire-grad-v1.0.1)
        // We need to find this directory and move its contents
        std::string extracted_root;
        for (const auto& entry : fs::directory_iterator(extract_dir)) {
            if (fs::is_directory(entry.path())) {
                extracted_root = entry.path().string();
                break;
            }
        }
        
        if (extracted_root.empty()) {
            std::cerr << "[!] No extracted directory found" << std::endl;
            return false;
        }
        
        // Move contents to plugin directory
        fs::path plugin_dir = get_plugin_dir() / plugin_name;
        fs::create_directories(plugin_dir);
        
        for (const auto& entry : fs::directory_iterator(extracted_root)) {
            fs::path dest = plugin_dir / entry.path().filename();
            if (fs::exists(dest)) {
                fs::remove_all(dest);
            }
            fs::rename(entry.path(), dest);
        }
        
        // Cleanup
        fs::remove_all(zip_path);
        fs::remove_all(extract_dir);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[!] Download error: " << e.what() << std::endl;
        // Cleanup on error
        cleanup_cache();
        return false;
    }
}

// Fetch the latest version tag from a GitHub repository using the GitHub API
inline std::string fetch_latest_version_from_repo(const std::string& repo_url) {
    if (g_offline) {
        return "";
    }
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
                            
                            if (highest_version.empty() || semver::is_older(highest_version, ver)) {
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
        
    } catch (const std::exception& e) {
        std::cerr << termcolor::yellow << "[!] Version fetch error: " << e.what() << termcolor::reset << std::endl;
    }
    
    return "";
}

} // namespace topaz

#endif // TOPAZ_DOWNLOADER_HPP
