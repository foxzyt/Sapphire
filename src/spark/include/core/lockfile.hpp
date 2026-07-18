#ifndef SPARK_LOCKFILE_HPP
#define SPARK_LOCKFILE_HPP

#include "types.hpp"
#include "fs_utils.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include "../third_party/json.hpp"

using json = nlohmann::json;

namespace spark {

// Structure to represent a locked dependency
struct LockedDependency {
    std::string name;
    std::string version;
    std::string checksum;
    std::string source; // registry or local
    std::vector<std::string> dependencies; // Direct dependencies
    
    // Convert to JSON
    json to_json() const {
        json j;
        j["name"] = name;
        j["version"] = version;
        j["checksum"] = checksum;
        j["source"] = source;
        j["dependencies"] = dependencies;
        return j;
    }
    
    // Convert from JSON
    static LockedDependency from_json(const json& j) {
        LockedDependency dep;
        dep.name = j["name"];
        dep.version = j["version"];
        dep.checksum = j["checksum"];
        dep.source = j["source"];
        if (j.contains("dependencies")) {
            dep.dependencies = j["dependencies"].get<std::vector<std::string>>();
        }
        return dep;
    }
};

// Lock file manager
class LockFile {
private:
    std::string project_name_;
    std::vector<LockedDependency> dependencies_;
    fs::path lockfile_path_;
    std::unordered_map<std::string, std::string> file_checksums_; // For CHECKSUMS.txt support
    
    // Calculate SHA256 checksum of a file
    std::string calculate_checksum(const fs::path& file_path) {
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
    }
    
public:
    LockFile(const fs::path& version_dir, const std::string& plugin_name, const std::string& version)
        : project_name_(plugin_name + "@" + version), lockfile_path_(version_dir / "spark.lock") {}
    
    // Add a dependency to the lock file
    void add_dependency(const std::string& name, const std::string& version, 
                      const std::string& source = "registry") {
        // Skip checksum calculation for "latest" version
        if (version == "latest") {
            LockedDependency dep;
            dep.name = name;
            dep.version = version;
            dep.checksum = "latest";
            dep.source = source;
            dep.dependencies = {};
            dependencies_.push_back(dep);
            return;
        }
        
        fs::path main_file = get_plugin_dir() / name / "versions" / ("v" + version) / "files" / "main.sp";
        std::string checksum = calculate_checksum(main_file);
        
        if (checksum.empty()) {
            std::cerr << "[!] Warning: Could not calculate checksum for " << name << " v" << version << std::endl;
            checksum = "unknown";
        }
        
        // Read dependencies from the version-specific PLUGIN.txt
        fs::path version_plugin_txt = get_plugin_dir() / name / "versions" / ("v" + version) / "PLUGIN.txt";
        auto version_meta = parse_plugin_txt(version_plugin_txt);
        
        // Also check DEPENDENCIES.txt in the version directory
        fs::path deps_file = get_plugin_dir() / name / "versions" / ("v" + version) / "DEPENDENCIES.txt";
        auto deps = parse_dependencies_txt(deps_file);
        
        std::vector<std::string> dep_names;
        for (const auto& dep : deps) {
            dep_names.push_back(dep.name + ":" + dep.version);
        }
        
        LockedDependency dep;
        dep.name = name;
        dep.version = version;
        dep.checksum = checksum;
        dep.source = source;
        dep.dependencies = dep_names;
        
        dependencies_.push_back(dep);
    }
    
    // Add file checksum to lock file
    void add_file_checksum(const std::string& file_path, const std::string& checksum) {
        // This will be used for CHECKSUMS.txt support
        file_checksums_[file_path] = checksum;
    }
    
    // Write lock file to disk
    bool write() {
        json j;
        j["project"] = project_name_;
        j["dependencies"] = json::array();
        
        for (const auto& dep : dependencies_) {
            j["dependencies"].push_back(dep.to_json());
        }
        
        // Add file checksums if any
        if (!file_checksums_.empty()) {
            j["checksums"] = json::object();
            for (const auto& [file_path, checksum] : file_checksums_) {
                j["checksums"][file_path] = checksum;
            }
        }
        
        std::ofstream file(lockfile_path_);
        if (!file.is_open()) {
            std::cerr << "[!] Failed to open lock file for writing: " << lockfile_path_ << std::endl;
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        std::cout << "[*] Lock file written to: " << lockfile_path_ << std::endl;
        
        // Also write CHECKSUMS.txt for compatibility
        write_checksums_txt();
        
        return true;
    }
    
    // Write CHECKSUMS.txt file
    bool write_checksums_txt() {
        fs::path checksums_path = lockfile_path_.parent_path() / "CHECKSUMS.txt";
        std::ofstream file(checksums_path);
        
        if (!file.is_open()) {
            std::cerr << "[!] Failed to open CHECKSUMS.txt for writing" << std::endl;
            return false;
        }
        
        file << "# SHA256 Checksums for " << project_name_ << "\n";
        file << "# Generated by Spark Package Manager\n\n";
        
        for (const auto& dep : dependencies_) {
            fs::path main_file = get_plugin_dir() / dep.name / "versions" / ("v" + dep.version) / "files" / "main.sp";
            if (fs::exists(main_file)) {
                std::string relative_path = "files/main.sp";
                file << dep.checksum << "  " << relative_path << "\n";
            }
        }
        
        // Add additional file checksums if any
        for (const auto& [file_path, checksum] : file_checksums_) {
            file << checksum << "  " << file_path << "\n";
        }
        
        file.close();
        return true;
    }
    
    // Read lock file from disk
    bool read() {
        if (!fs::exists(lockfile_path_)) {
            return false;
        }
        
        std::ifstream file(lockfile_path_);
        if (!file.is_open()) {
            std::cerr << "[!] Failed to open lock file for reading: " << lockfile_path_ << std::endl;
            return false;
        }
        
        try {
            json j;
            file >> j;
            
            project_name_ = j["project"];
            dependencies_.clear();
            
            if (j.contains("dependencies")) {
                for (const auto& dep_json : j["dependencies"]) {
                    dependencies_.push_back(LockedDependency::from_json(dep_json));
                }
            }
            
            file.close();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[!] Failed to parse lock file: " << e.what() << std::endl;
            file.close();
            return false;
        }
    }
    
    // Verify checksums of all locked dependencies
    bool verify() {
        bool all_valid = true;
        
        for (const auto& dep : dependencies_) {
            fs::path main_file = get_plugin_dir() / dep.name / "versions" / ("v" + dep.version) / "files" / "main.sp";
            std::string current_checksum = calculate_checksum(main_file);
            
            if (current_checksum != dep.checksum) {
                std::cerr << "[!] Checksum mismatch for " << dep.name << " v" << dep.version << std::endl;
                std::cerr << "    Expected: " << dep.checksum << std::endl;
                std::cerr << "    Got:      " << current_checksum << std::endl;
                all_valid = false;
            }
        }
        
        return all_valid;
    }
    
    // Get all dependencies
    const std::vector<LockedDependency>& get_dependencies() const {
        return dependencies_;
    }
    
    // Clear all dependencies
    void clear() {
        dependencies_.clear();
    }
};

} // namespace spark

#endif // SPARK_LOCKFILE_HPP
