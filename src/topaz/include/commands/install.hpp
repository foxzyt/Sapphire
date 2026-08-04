#ifndef TOPAZ_COMMANDS_INSTALL_HPP
#define TOPAZ_COMMANDS_INSTALL_HPP

#include "core/types.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/downloader.hpp"
#include "core/resolver.hpp"
#include "core/semver.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include "termcolor.hpp"

namespace topaz {
namespace commands {

// --- SemVer: aliases de compatibilidade (lógica centralizada em core/semver.hpp) ---
inline int install_compare_versions(const std::string& v1, const std::string& v2) {
    return semver::compare(v1, v2);
}

inline bool install_is_version_older(const std::string& v1, const std::string& v2) {
    return semver::is_older(v1, v2);
}

inline bool install_satisfies_constraint(const std::string& version, const std::string& constraint) {
    return semver::satisfies(version, constraint);
}

// Busca todas as versoes de um plugin no GitHub e retorna a maior que satisfaz a constraint
inline std::string install_resolve_github_version(const std::string& repo_url, const std::string& constraint) {
    std::string api_url = repo_url;
    size_t github_pos = api_url.find("github.com/");
    if (github_pos == std::string::npos) return "";

    std::string owner_repo = api_url.substr(github_pos + 11);
    if (!owner_repo.empty() && owner_repo.back() == '/') owner_repo.pop_back();
    if (owner_repo.size() > 4 && owner_repo.substr(owner_repo.size() - 4) == ".git") {
        owner_repo = owner_repo.substr(0, owner_repo.size() - 4);
    }

    // Armazena <VersaoLimpa, NomeRealDaPasta> -> Ex: <"1.1.0", "v1.1.0">
    std::vector<std::pair<std::string, std::string>> all_versions;

    try {
        httplib::Client cli("api.github.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(15);


        httplib::Headers headers = {
            {"User-Agent", "Sapphire-Mine/2.2"},
            {"Accept", "application/vnd.github.v3+json"}
        };

        // 1. Tenta pegar pela estrutura nativa de pastas versions/
        std::string contents_path = "/repos/" + owner_repo + "/contents/versions";
        auto contents_res = cli.Get(contents_path.c_str(), headers);
        if (contents_res && contents_res->status == 200) {
            try {
                nlohmann::json items = nlohmann::json::parse(contents_res->body);
                if (items.is_array()) {
                    for (const auto& item : items) {
                        if (item.contains("type") && item["type"] == "dir" && item.contains("name")) {
                            std::string raw_ver = item["name"].get<std::string>();
                            std::string clean_ver = semver::normalize(raw_ver);
                            all_versions.push_back({clean_ver, raw_ver});
                        }
                    }
                }
            } catch (...) {}
        }

        // 2. Fallback para Tags caso nao use estrutura nativa
        if (all_versions.empty()) {
            std::string tags_path = "/repos/" + owner_repo + "/tags";
            auto tags_res = cli.Get(tags_path.c_str(), headers);
            if (tags_res && tags_res->status == 200) {
                try {
                    nlohmann::json tags = nlohmann::json::parse(tags_res->body);
                    if (tags.is_array()) {
                        for (const auto& tag_obj : tags) {
                            std::string raw_ver = tag_obj["name"].get<std::string>();
                            std::string clean_ver = semver::normalize(raw_ver);
                            all_versions.push_back({clean_ver, raw_ver});
                        }
                    }
                } catch (...) {}
            }
        }

        // 3. Fallback para Releases Oficiais
        if (all_versions.empty()) {
            std::string rel_path = "/repos/" + owner_repo + "/releases";
            auto rel_res = cli.Get(rel_path.c_str(), headers);
            if (rel_res && rel_res->status == 200) {
                try {
                    nlohmann::json rels = nlohmann::json::parse(rel_res->body);
                    if (rels.is_array()) {
                        for (const auto& rel_obj : rels) {
                            if (rel_obj.contains("tag_name")) {
                                std::string raw_ver = rel_obj["tag_name"].get<std::string>();
                                std::string clean_ver = semver::normalize(raw_ver);
                                all_versions.push_back({clean_ver, raw_ver});
                            }
                        }
                    }
                } catch (...) {}
            }
        }
    } catch (...) {}

    // Usa semver::resolve_best para encontrar a melhor versão
    std::vector<std::string> clean_list;
    clean_list.reserve(all_versions.size());
    for (const auto& p : all_versions) clean_list.push_back(p.first);

    std::string best_clean = semver::resolve_best(clean_list, constraint);
    if (best_clean.empty()) return "";

    // Retorna o nome raw correspondente (com prefixo 'v' se existia)
    for (const auto& p : all_versions) {
        if (p.first == best_clean) return p.second;
    }

    return semver::with_v(best_clean);
}

// Forward declaration of project install helper
inline int cmd_install_project(bool local_scope = false, bool frozen_lockfile = false, bool no_save = false) {
    fs::path manifest_path = get_project_manifest_path();
    if (!fs::exists(manifest_path)) {
        std::cerr << termcolor::red << "[!] No sapphire.json manifest found in current directory." << termcolor::reset << std::endl;
        std::cerr << "    Run 'topaz init' to create one." << std::endl;
        return 1;
    }
    
    // Backup manifests for rollback
    std::string manifest_backup;
    if (fs::exists(manifest_path)) {
        std::ifstream f_in(manifest_path);
        std::stringstream buffer;
        buffer << f_in.rdbuf();
        manifest_backup = buffer.str();
    }
    
    std::string lockfile_backup;
    fs::path lockfile_path = get_project_lockfile_path();
    bool has_lockfile = fs::exists(lockfile_path);
    if (has_lockfile) {
        std::ifstream f_in(lockfile_path);
        std::stringstream buffer;
        buffer << f_in.rdbuf();
        lockfile_backup = buffer.str();
    }
    
    nlohmann::json manifest;
    try {
        std::ifstream f(manifest_path);
        f >> manifest;
        f.close();
    } catch (const std::exception& e) {
        std::cerr << termcolor::red << "[!] Failed to parse sapphire.json: " << e.what() << termcolor::reset << std::endl;
        return 1;
    }
    
    std::string proj_name = "project";
    if (manifest.contains("name") && manifest["name"].is_string()) {
        proj_name = manifest["name"].get<std::string>();
    }
    std::string proj_ver = "1.0.0";
    if (manifest.contains("version") && manifest["version"].is_string()) {
        proj_ver = manifest["version"].get<std::string>();
    }
    
    
    if (frozen_lockfile && !has_lockfile) {
        std::cerr << termcolor::red << "[!] --frozen-lockfile specified but no topaz.lock exists." << termcolor::reset << std::endl;
        return 1;
    }
    
    DependencyResolver resolver(fs::current_path(), proj_name, local_scope);
    resolver.set_no_save(no_save);
    resolver.set_frozen_lockfile(frozen_lockfile);
    
    if (has_lockfile) {
        std::cout << termcolor::cyan << "[*] Installing dependencies from lockfile..." << termcolor::reset << std::endl;
        
        nlohmann::json lock_json;
        try {
            std::ifstream lf(lockfile_path);
            lf >> lock_json;
            lf.close();
        } catch (const std::exception& e) {
            std::cerr << termcolor::red << "[!] Failed to parse topaz.lock: " << e.what() << termcolor::reset << std::endl;
            return 1;
        }
        
        if (lock_json.contains("dependencies")) {
            for (const auto& dep : lock_json["dependencies"]) {
                std::string dep_name = dep["name"].get<std::string>();
                std::string dep_version = dep["version"].get<std::string>();
                
                std::cout << termcolor::cyan << "[*] Installing locked version: " << dep_name << " v" << dep_version << termcolor::reset << std::endl;
                
                auto registry_entry = query_registry(dep_name);
                if (!registry_entry) {
                    std::cerr << termcolor::red << "[!] Lockfile dependency '" << dep_name << "' not found in registry" << termcolor::reset << std::endl;
                    return 1;
                }
                
                if (!download_and_extract_plugin(dep_name, registry_entry->repository, "v" + dep_version, registry_entry->checksum)) {
                    std::cerr << termcolor::red << "[!] Failed to install " << dep_name << " v" << dep_version << termcolor::reset << std::endl;
                    // Rollback
                    std::cerr << termcolor::yellow << "[*] Rolling back installation..." << termcolor::reset << std::endl;
                    if (!manifest_backup.empty()) { std::ofstream out(manifest_path); out << manifest_backup; }
                    if (has_lockfile && !lockfile_backup.empty()) { std::ofstream out(lockfile_path); out << lockfile_backup; }
                    return 1;
                }
                
                resolver.resolve_and_install(dep_name, dep_version);
            }
        }
    } else {
        std::cout << termcolor::cyan << "[*] Resolving dependencies from sapphire.json..." << termcolor::reset << std::endl;
        
        std::vector<std::pair<std::string, std::string>> deps_to_resolve;
        if (manifest.contains("dependencies") && manifest["dependencies"].is_object()) {
            for (auto& [name, val] : manifest["dependencies"].items()) {
                if (val.is_string()) {
                    deps_to_resolve.push_back({name, val.get<std::string>()});
                }
            }
        }
        if (manifest.contains("devDependencies") && manifest["devDependencies"].is_object()) {
            for (auto& [name, val] : manifest["devDependencies"].items()) {
                if (val.is_string()) {
                    deps_to_resolve.push_back({name, val.get<std::string>()});
                }
            }
        }
        
        for (const auto& [name, constraint] : deps_to_resolve) {
            std::cout << termcolor::cyan << "[*] Resolving " << name << " (" << constraint << ")..." << termcolor::reset << std::endl;
            
            auto registry_entry = query_registry(name);
            if (!registry_entry) {
                std::cerr << termcolor::red << "[!] Dependency '" << name << "' not found in registry" << termcolor::reset << std::endl;
                return 1;
            }
            
            std::string resolved_version = constraint;
            bool is_semver_query = (constraint == "latest" || constraint[0] == '^' || constraint[0] == '>' || constraint[0] == '<' || constraint[0] == '~');
            
            if (is_semver_query) {
                std::string gh_resolved = install_resolve_github_version(registry_entry->repository, constraint);
                if (!gh_resolved.empty()) {
                    resolved_version = gh_resolved;
                } else {
                    std::cerr << termcolor::red << "[!] Could not resolve constraint '" << constraint << "' on GitHub" << termcolor::reset << std::endl;
                    return 1;
                }
            } else if (!resolved_version.empty() && std::isdigit(resolved_version[0])) {
                resolved_version = "v" + resolved_version;
            }
            
            std::string tag_version = resolved_version;
            if (tag_version.size() > 0 && tag_version[0] == 'v') tag_version = tag_version.substr(1);
            
            if (!download_and_extract_plugin(name, registry_entry->repository, "v" + tag_version, registry_entry->checksum)) {
                std::cerr << termcolor::red << "[!] Failed to download " << name << " v" << tag_version << termcolor::reset << std::endl;
                // Rollback
                std::cerr << termcolor::yellow << "[*] Rolling back installation..." << termcolor::reset << std::endl;
                if (!manifest_backup.empty()) { std::ofstream out(manifest_path); out << manifest_backup; }
                if (has_lockfile && !lockfile_backup.empty()) { std::ofstream out(lockfile_path); out << lockfile_backup; }
                return 1;
            }
            
            resolver.resolve_and_install(name, resolved_version);
        }
    }
    
    // Copy all installed dependencies to local scope if requested
    if (local_scope) {
        for (const auto& [name, ver] : resolver.get_installed_plugins()) {
            fs::path target_local = get_local_plugin_dir() / name;
            fs::path global_plugin = get_plugin_dir() / name;
            
            if (fs::exists(global_plugin)) {
                std::cout << termcolor::cyan << "[*] Copying " << name << " to local project scope..." << termcolor::reset << std::endl;
                if (fs::exists(target_local)) {
                    fs::remove_all(target_local);
                }
                fs::create_directories(target_local.parent_path());
                try {
                    fs::copy(global_plugin, target_local, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                } catch (const std::exception& e) {
                    std::cerr << termcolor::red << "[!] Failed to copy: " << e.what() << termcolor::reset << std::endl;
                }
            }
        }
    }
    
    // Write lockfiles for sub-versions (plugin level)
    resolver.write_lockfiles();
    
    // Write project-level lockfile
    if (!no_save) {
        LockFile project_lockfile(fs::current_path(), proj_name, proj_ver);
        for (const auto& [name, ver] : resolver.get_installed_plugins()) {
            project_lockfile.add_dependency(name, ver, "registry");
        }
        project_lockfile.write();
    }
    
    std::cout << termcolor::green << "[OK] Project dependencies successfully installed" << termcolor::reset << std::endl;
    return 0;
}

// Install command with dependency resolution and local/global scope support
inline int cmd_install(const std::string& plugin_name, 
                       const std::string& version = "latest", 
                       bool local_scope = false,
                       bool save_dev = false,
                       bool no_save = false,
                       bool frozen_lockfile = false) {
    if (plugin_name.empty()) {
        return cmd_install_project(local_scope, frozen_lockfile, no_save);
    }
    
    // Backup manifests for rollback
    fs::path manifest_path = get_project_manifest_path();
    std::string manifest_backup;
    if (fs::exists(manifest_path)) {
        std::ifstream f_in(manifest_path);
        std::stringstream buffer;
        buffer << f_in.rdbuf();
        manifest_backup = buffer.str();
    }
    
    std::string lockfile_backup;
    fs::path lockfile_path = get_project_lockfile_path();
    bool has_lockfile = fs::exists(lockfile_path);
    if (has_lockfile) {
        std::ifstream f_in(lockfile_path);
        std::stringstream buffer;
        buffer << f_in.rdbuf();
        lockfile_backup = buffer.str();
    }
    
    
    std::cout << termcolor::cyan << "[*] Installing plugin: " << plugin_name << termcolor::reset;
    if (version != "latest") {
        std::cout << " (version: " << version << ")";
    }
    std::cout << std::endl;
    
    // Query the registry first
    auto registry_entry = query_registry(plugin_name);
    if (!registry_entry) {
        std::cerr << termcolor::red << "[!] Plugin '" << plugin_name << "' not found in central registry" << termcolor::reset << std::endl;
        return 1;
    }
    
    std::cout << termcolor::blue << "[*] Repository: " << registry_entry->repository << termcolor::reset << std::endl;
    
    // Resolve version dynamically
    std::string resolved_version = version;
    bool is_semver_query = (version == "latest" || version[0] == '^' || version[0] == '>' || version[0] == '<' || version[0] == '~');
    
    if (is_semver_query) {
        std::cout << termcolor::cyan << "[*] Resolving version constraint '" << version << "' from GitHub..." << termcolor::reset << std::endl;
        std::string gh_resolved = install_resolve_github_version(registry_entry->repository, version);
        
        if (!gh_resolved.empty()) {
            resolved_version = gh_resolved;
            std::cout << termcolor::green << "[*] Resolved '" << version << "' to " << resolved_version << termcolor::reset << std::endl;
        } else {
            std::cout << termcolor::red << "[!] Could not find any version matching constraint '" << version << "' on GitHub" << termcolor::reset << std::endl;
            return 1;
        }
    } else {
        if (!resolved_version.empty() && std::isdigit(resolved_version[0])) {
            resolved_version = "v" + resolved_version;
        }
    }

    // Check if frozen-lockfile prevents installing a new version
    if (frozen_lockfile) {
        fs::path lockfile_path = get_project_lockfile_path();
        if (fs::exists(lockfile_path)) {
            nlohmann::json lock_json;
            std::ifstream lf(lockfile_path);
            lf >> lock_json;
            lf.close();
            
            bool locked_match = false;
            if (lock_json.contains("dependencies")) {
                for (const auto& dep : lock_json["dependencies"]) {
                    if (dep["name"] == plugin_name && dep["version"] == semver::normalize(resolved_version)) {
                        locked_match = true;
                        break;
                    }
                }
            }
            if (!locked_match) {
                std::cerr << termcolor::red << "[!] --frozen-lockfile error: Cannot install new dependency '" << plugin_name << "' not matching locked state." << termcolor::reset << std::endl;
                return 1;
            }
        }
    }

    // Target directory
    fs::path target_plugin_dir;
    if (local_scope) {
        target_plugin_dir = get_local_plugin_dir() / plugin_name;
        std::cout << termcolor::yellow << "[*] Target: local project scope (./plugins/)" << termcolor::reset << std::endl;
    } else {
        target_plugin_dir = get_plugin_dir() / plugin_name;
        std::cout << termcolor::yellow << "[*] Target: global scope (AppData)" << termcolor::reset << std::endl;
    }
    
    // Check if already installed
    if (local_scope ? is_plugin_installed_local(plugin_name) : is_plugin_installed(plugin_name)) {
        fs::path plugin_path = (local_scope ? get_local_plugin_dir() : get_plugin_dir()) / plugin_name / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_path);
        if (meta) {
            std::string clean_resolved = resolved_version;
            if (clean_resolved.size() > 0 && clean_resolved[0] == 'v') clean_resolved = clean_resolved.substr(1);
            
            std::cout << termcolor::yellow << "[*] Plugin already installed in " 
                      << (local_scope ? "local" : "global") << " scope (Current: v" << meta->version << " | Target: v" << clean_resolved << ")" << termcolor::reset << std::endl;
            
            if (meta->version != clean_resolved) {
                if (meta->deprecated) {
                    std::cout << termcolor::red << "[!] WARNING: This plugin is DEPRECATED" << termcolor::reset << std::endl;
                }
                if (!meta->notice.empty()) {
                    std::cout << termcolor::yellow << "[!] NOTICE: " << meta->notice << termcolor::reset << std::endl;
                }
                
                std::cout << "Continue installation/update? (Y/n): ";
                std::string response;
                std::getline(std::cin, response);
                response = trim(response);
                
                if (response == "n" || response == "N") {
                    std::cout << termcolor::yellow << "[*] Installation cancelled" << termcolor::reset << std::endl;
                    return 0;
                }
            } else {
                std::cout << termcolor::green << "[*] Target version v" << clean_resolved << " is already the active version. Processing dependencies..." << termcolor::reset << std::endl;
            }
        }
    }
    
    std::string tag_version = resolved_version;
    if (tag_version.size() > 0 && tag_version[0] == 'v') tag_version = tag_version.substr(1);
    std::cout << termcolor::cyan << "[*] Fetching version " << tag_version << "..." << termcolor::reset << std::endl;
    
    if (!download_and_extract_plugin(plugin_name, registry_entry->repository, "v" + tag_version, registry_entry->checksum)) {
        std::cerr << termcolor::red << "[!] Failed to download and extract plugin." << termcolor::reset << std::endl;
        std::cerr << termcolor::yellow << "[*] Rolling back installation..." << termcolor::reset << std::endl;
        if (!manifest_backup.empty()) { std::ofstream out(manifest_path); out << manifest_backup; }
        if (has_lockfile && !lockfile_backup.empty()) { std::ofstream out(lockfile_path); out << lockfile_backup; }
        return 1;
    }
    
    DependencyResolver resolver(fs::current_path(), plugin_name, local_scope);
    resolver.set_no_save(no_save);
    resolver.set_frozen_lockfile(frozen_lockfile);
    resolver.resolve_and_install(plugin_name, resolved_version);
    
    resolver.write_lockfiles();
    
    if (local_scope) {
        fs::path global_plugin = get_plugin_dir() / plugin_name;
        if (fs::exists(global_plugin)) {
            std::cout << termcolor::cyan << "[*] Copying to local project scope..." << termcolor::reset << std::endl;
            if (fs::exists(target_plugin_dir)) {
                fs::remove_all(target_plugin_dir);
            }
            fs::create_directories(target_plugin_dir.parent_path());
            try {
                fs::copy(global_plugin, target_plugin_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                std::cout << termcolor::green << "[+] Copied to local project scope" << termcolor::reset << std::endl;
            } catch (const std::exception& e) {
                std::cerr << termcolor::red << "[!] Failed to copy to local scope: " << e.what() << termcolor::reset << std::endl;
            }
        }
    }
    
    fs::path target_base = local_scope ? target_plugin_dir : (get_plugin_dir() / plugin_name);
    fs::path target_plugin_txt = target_base / "PLUGIN.txt";
    if (fs::exists(target_plugin_txt)) {
        auto updated_meta = parse_plugin_txt(target_plugin_txt);
        if (updated_meta) {
            std::string save_ver = resolved_version;
            if (save_ver.size() > 0 && save_ver[0] == 'v') save_ver = save_ver.substr(1);
            updated_meta->version = save_ver;
            write_plugin_txt(target_plugin_txt, *updated_meta);
        }
    }
    
    // Save to sapphire.json and project-level topaz.lock if not no_save
    if (!no_save && is_sapphire_project()) {
        nlohmann::json manifest;
        fs::path manifest_path = get_project_manifest_path();
        if (fs::exists(manifest_path)) {
            try {
                std::ifstream f(manifest_path);
                f >> manifest;
                f.close();
            } catch (...) {}
        }
        
        if (!manifest.contains("name")) manifest["name"] = fs::current_path().filename().string();
        if (!manifest.contains("version")) manifest["version"] = "1.0.0";
        
        std::string target_key = save_dev ? "devDependencies" : "dependencies";
        if (!manifest.contains(target_key)) manifest[target_key] = nlohmann::json::object();
        
        std::string clean_ver = resolved_version;
        if (clean_ver.size() > 0 && clean_ver[0] == 'v') clean_ver = clean_ver.substr(1);
        std::string constraint = (version == "latest" || is_semver_query) ? ("^" + clean_ver) : version;
        
        manifest[target_key][plugin_name] = constraint;
        
        std::ofstream out(manifest_path);
        out << manifest.dump(2);
        out.close();
        std::cout << "[*] Updated " << manifest_path.filename().string() << " dependencies" << std::endl;
        
        // Write project-level lockfile
        LockFile project_lockfile(fs::current_path(), manifest["name"].get<std::string>(), manifest["version"].get<std::string>());
        // Add this plugin and its transitives
        project_lockfile.add_dependency(plugin_name, clean_ver, "registry");
        for (const auto& [name, ver] : resolver.get_installed_plugins()) {
            project_lockfile.add_dependency(name, ver, "registry");
        }
        project_lockfile.write();
    }
    
    bool success = false;
    std::string clean_version = resolved_version;
    if (clean_version.size() > 0 && clean_version[0] == 'v') {
        clean_version = clean_version.substr(1);
    }
    
    fs::path version_dir_v = get_plugin_version_path(plugin_name, "v" + clean_version);
    fs::path version_dir_clean = get_plugin_version_path(plugin_name, clean_version);
    success = fs::exists(version_dir_v) || fs::exists(version_dir_clean);
    
    if (success) {
        std::cout << termcolor::green << "[OK] Installed '" << plugin_name << "'" << termcolor::reset;
        if (resolver.get_total_installed() > 0) {
            std::cout << " and " << resolver.get_total_installed() << " dependencies";
        }
        std::cout << std::endl;
        return 0;
    } else {
        std::cerr << termcolor::red << "[!] Installation failed" << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_INSTALL_HPP
