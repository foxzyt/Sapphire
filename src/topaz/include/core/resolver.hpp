#ifndef TOPAZ_RESOLVER_HPP
#define TOPAZ_RESOLVER_HPP

#include "types.hpp"
#include "fs_utils.hpp"
#include "parser.hpp"
#include "downloader.hpp"
#include "lockfile.hpp"
#include <unordered_set>
#include <iostream>
#include "termcolor.hpp"
#ifdef OPENSSL_FOUND
#include <openssl/evp.h>
#endif
#include <iomanip>
#include <sstream>

namespace topaz {

// Dependency resolution engine using DFS to avoid loops
class DependencyResolver {
private:
    std::unordered_set<std::string> visited_;
    int total_installed_ = 0;
    std::string project_name_;
    fs::path project_dir_;
    
    // Track all installed plugins with their versions for lockfile
    std::vector<std::pair<std::string, std::string>> installed_plugins_;
    std::vector<std::pair<std::string, std::string>> newly_installed_plugins_;
    
    // Track version conflicts: plugin_name -> set of required versions
    std::unordered_map<std::string, std::set<std::string>> version_conflicts_;
    
    // Calculate SHA256 checksum of a file
    std::string calculate_checksum(const fs::path& file_path) {
#ifdef OPENSSL_FOUND
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
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
        
        return ss.str();
#else
        return ""; // Hash not available without OpenSSL
#endif
    }
    
    // Check for version conflicts
    bool check_version_conflict(const std::string& plugin_name, const std::string& version) {
        auto it = version_conflicts_.find(plugin_name);
        if (it != version_conflicts_.end()) {
            // This plugin has been requested before
            if (it->second.count(version) == 0) {
                // Different version requested - conflict!
                std::cerr << termcolor::red << "[!] Version conflict for plugin '" << plugin_name << "'" << termcolor::reset << std::endl;
                std::cerr << termcolor::yellow << "    Requested versions: ";
                for (const auto& v : it->second) {
                    std::cerr << v << " ";
                }
                std::cerr << "and " << version << termcolor::reset << std::endl;
                std::cerr << termcolor::yellow << "    Both versions will be installed. Use import with version specifier to choose." << termcolor::reset << std::endl;
                // Don't return true - allow both versions to coexist
                return false;
            }
        }
        return false;
    }
    
    // Record version requirement
    void record_version_requirement(const std::string& plugin_name, const std::string& version) {
        version_conflicts_[plugin_name].insert(version);
    }
    
    // Get all available versions for a plugin
    std::vector<std::string> get_available_versions(const std::string& plugin_name) {
        std::vector<std::string> versions;
        fs::path versions_dir = get_plugin_dir() / plugin_name / "versions";
        
        if (fs::exists(versions_dir) && fs::is_directory(versions_dir)) {
            for (const auto& entry : fs::directory_iterator(versions_dir)) {
                if (fs::is_directory(entry.path())) {
                    std::string version_name = entry.path().filename().string();
                    // Remove 'v' prefix if present
                    if (version_name.size() > 0 && version_name[0] == 'v') {
                        version_name = version_name.substr(1);
                    }
                    versions.push_back(version_name);
                }
            }
        }
        
        return versions;
    }
    
    // Install a specific version and generate its lockfile
    void install_version(const std::string& plugin_name, const std::string& version) {
        // Remove 'v' prefix if present
        std::string clean_version = version;
        if (clean_version.size() > 0 && clean_version[0] == 'v') {
            clean_version = clean_version.substr(1);
        }
        
        // Check if version exists locally after downloading main branch
        fs::path version_dir = get_plugin_dir() / plugin_name / "versions" / ("v" + clean_version);
        
        if (!fs::exists(version_dir)) {
            std::cerr << termcolor::red << "[!] Version " << clean_version << " not found for plugin " << plugin_name << termcolor::reset << std::endl;
            std::cerr << termcolor::yellow << "[!] Available versions: ";
            auto available = get_available_versions(plugin_name);
            for (const auto& v : available) {
                std::cerr << v << " ";
            }
            std::cerr << termcolor::reset << std::endl;
            return;
        }
        
        if (topaz::g_verbose) std::cout << termcolor::cyan << "[*] Processing " << plugin_name << " v" << clean_version << termcolor::reset << std::endl;
        
        // Create lockfile for this specific version
        LockFile version_lockfile(version_dir, plugin_name, clean_version);
        
        // Add this plugin itself to its lockfile
        fs::path main_file = version_dir / "files" / "main.sp";
        std::string checksum = calculate_checksum(main_file);
        
        if (!checksum.empty()) {
            version_lockfile.add_dependency(plugin_name, clean_version, "installed");
        }
        
        // Read dependencies from version-specific DEPENDENCIES.txt
        fs::path deps_path = version_dir / "DEPENDENCIES.txt";
        auto dependencies = parse_dependencies_txt(deps_path);
        
        if (!dependencies.empty()) {
            if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Resolving " << dependencies.size() << " dependencies for " << plugin_name << " v" << clean_version << termcolor::reset << std::endl;
        }
        
        // Add dependencies to lockfile and resolve them
        for (const auto& dep : dependencies) {
            version_lockfile.add_dependency(dep.name, dep.version, "registry");
            resolve_and_install(dep.name, dep.version);
        }
        
        // Write lockfile for this version
        if (!no_save_) {
            version_lockfile.write();
        }
    }
    
    bool local_scope_ = false;
    bool no_save_ = false;
    bool frozen_lockfile_ = false;
    
public:
    DependencyResolver(const fs::path& project_dir = fs::current_path(), 
                      const std::string& project_name = "sapphire-project",
                      bool local_scope = false)
        : project_name_(project_name), project_dir_(project_dir), local_scope_(local_scope) {}
    
    // Enable local scope installation
    void set_local_scope(bool local) {
        local_scope_ = local;
    }
    
    void set_no_save(bool no_save) {
        no_save_ = no_save;
    }
    
    void set_frozen_lockfile(bool frozen) {
        frozen_lockfile_ = frozen;
    }
    
    // Main resolution function
    void resolve_and_install(const std::string& plugin_name, const std::string& version = "latest") {
        // Check if already visited (avoid infinite loops)
        if (visited_.count(plugin_name)) {
            return;
        }
        
        // Record version requirement and check for conflicts
        record_version_requirement(plugin_name, version);
        if (check_version_conflict(plugin_name, version)) {
            // Version conflict detected - skip this dependency
            std::cerr << termcolor::yellow << "[*] Skipping " << plugin_name << " v" << version << " due to version conflict" << termcolor::reset << std::endl;
            return;
        }
        
        visited_.insert(plugin_name);
        
        if (is_plugin_installed(plugin_name)) {
            if (!topaz::g_verbose && plugin_name != project_name_) {
                std::cout << "  Dependency '" << plugin_name << "' already exists." << std::endl;
            }
            if (version == "latest") {
                // For latest, check if we have all versions
                auto available = get_available_versions(plugin_name);
                if (!available.empty()) {
                    if (topaz::g_verbose) {
                        std::cout << termcolor::yellow << "[*] Plugin '" << plugin_name << "' already installed with versions: ";
                        for (const auto& v : available) {
                            std::cout << v << " ";
                        }
                        std::cout << termcolor::reset << std::endl;
                    }
                    
                    // Add all versions to lockfile
                    for (const auto& ver : available) {
                        installed_plugins_.push_back({plugin_name, ver});
                    }
                    
                    // Process dependencies for all installed versions
                    for (const auto& ver : available) {
                        fs::path deps_path = get_plugin_dir() / plugin_name / "versions" / ("v" + ver) / "DEPENDENCIES.txt";
                        auto dependencies = parse_dependencies_txt(deps_path);
                        
                        if (!dependencies.empty()) {
                            if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Resolving " << dependencies.size() << " dependencies for " << plugin_name << " v" << ver << termcolor::reset << std::endl;
                        }
                        
                        for (const auto& dep : dependencies) {
                            resolve_and_install(dep.name, dep.version);
                        }
                    }
                    return;
                }
            } else {
                // For specific version, check if that version exists
                fs::path version_dir = get_plugin_dir() / plugin_name / "versions" / ("v" + version);
                if (fs::exists(version_dir)) {
                    // Version exists, but we still need to process its dependencies
                    if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Plugin '" << plugin_name << "' v" << version << " already installed, processing dependencies" << termcolor::reset << std::endl;
                    
                    // Add to lockfile
                    installed_plugins_.push_back({plugin_name, version});
                    
                    install_version(plugin_name, version);
                    return;
                }
            }
        }
        
        if (version == "latest") {
            // Download all available versions
            if (topaz::g_verbose) std::cout << termcolor::cyan << "[*] Installing all versions of: " << plugin_name << termcolor::reset << std::endl;
            
            // First download the main plugin to get version info
            auto registry_entry = query_registry(plugin_name);
            if (!registry_entry) {
                std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' not found in registry" << termcolor::reset << std::endl;
                return;
            }
            
            if (!topaz::g_verbose && plugin_name != project_name_) {
                std::cout << "  Downloading '" << plugin_name << "' (dependency)..." << std::endl;
            }
            
            // Download main branch (latest)
            if (!download_and_extract_plugin(plugin_name, registry_entry->repository, "latest")) {
                std::cerr << termcolor::red << "[!] Failed to download plugin: " << plugin_name << termcolor::reset << std::endl;
                return;
            }
            
            total_installed_++;
            
            // Read the PLUGIN.txt to get the actual version
            fs::path plugin_path = get_plugin_dir() / plugin_name / "PLUGIN.txt";
            auto meta = parse_plugin_txt(plugin_path);
            std::string actual_version = (meta && !meta->version.empty()) ? meta->version : "";
            
            if (actual_version.empty()) {
                // Try to find the version by looking at the versions directory
                auto versions = get_available_versions(plugin_name);
                if (!versions.empty()) {
                    actual_version = versions[0]; // Usually latest is the only one downloaded here
                } else {
                    actual_version = "latest";
                }
            }
            
            // Track for lockfile
            installed_plugins_.push_back({plugin_name, actual_version});
            newly_installed_plugins_.push_back({plugin_name, actual_version});
            
            // Get all available versions
            auto versions = get_available_versions(plugin_name);
            
            // Process dependencies for each version
            for (const auto& ver : versions) {
                fs::path deps_path = get_plugin_dir() / plugin_name / "versions" / ("v" + ver) / "DEPENDENCIES.txt";
                auto dependencies = parse_dependencies_txt(deps_path);
                
                if (!dependencies.empty()) {
                    if (topaz::g_verbose) std::cout << termcolor::yellow << "[*] Resolving " << dependencies.size() << " dependencies for " << plugin_name << " v" << ver << termcolor::reset << std::endl;
                }
                
                for (const auto& dep : dependencies) {
                    resolve_and_install(dep.name, dep.version);
                }
            }
            
        } else {
            // Install specific version - first ensure plugin is downloaded
            auto registry_entry = query_registry(plugin_name);
            if (!registry_entry) {
                std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' not found in registry" << termcolor::reset << std::endl;
                return;
            }
            
            if (!topaz::g_verbose && plugin_name != project_name_) {
                std::cout << "  Downloading '" << plugin_name << "' (dependency)..." << std::endl;
            }
            
            // Try downloading specific version if not already installed
            if (!is_plugin_installed(plugin_name)) {
                if (!download_and_extract_plugin(plugin_name, registry_entry->repository, "latest")) {
                    std::cerr << termcolor::red << "[!] Failed to download plugin: " << plugin_name << termcolor::reset << std::endl;
                    return;
                }
                total_installed_++;
                
                // Track for lockfile - add all versions
                auto versions = get_available_versions(plugin_name);
                for (const auto& ver : versions) {
                    installed_plugins_.push_back({plugin_name, ver});
                    newly_installed_plugins_.push_back({plugin_name, ver});
                }
            }
            
            // Process the specific version
            install_version(plugin_name, version);
        }
    }
    
    // Write lock files for all installed versions
    bool write_lockfiles() {
        if (no_save_) {
            return true;
        }
        for (const auto& [plugin_name, version] : installed_plugins_) {
            fs::path version_dir = get_plugin_dir() / plugin_name / "versions" / ("v" + version);
            install_version(plugin_name, version);
        }
        
        if (topaz::g_verbose) std::cout << "[*] Lock files written for all installed versions" << std::endl;
        return true;
    }
    
    int get_total_installed() const {
        return total_installed_;
    }
    
    const std::vector<std::pair<std::string, std::string>>& get_installed_plugins() const {
        return installed_plugins_;
    }
    
    const std::vector<std::pair<std::string, std::string>>& get_newly_installed_plugins() const {
        return newly_installed_plugins_;
    }
    
    void reset() {
        visited_.clear();
        total_installed_ = 0;
        version_conflicts_.clear();
        installed_plugins_.clear();
    }
};

// Convenience function for single-call resolution
inline int install_plugin_with_dependencies(const std::string& plugin_name, const std::string& version = "latest") {
    DependencyResolver resolver;
    resolver.resolve_and_install(plugin_name, version);
    return resolver.get_total_installed();
}

} // namespace topaz

#endif // TOPAZ_RESOLVER_HPP
