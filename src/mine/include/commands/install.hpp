#ifndef MINE_COMMANDS_INSTALL_HPP
#define MINE_COMMANDS_INSTALL_HPP

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

namespace mine {
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
        httplib::SSLClient cli("api.github.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(15);
        cli.enable_server_certificate_verification(false);

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

// Install command with dependency resolution and local/global scope support
inline int cmd_install(const std::string& plugin_name, const std::string& version = "latest", bool local_scope = false) {
    if (plugin_name.empty()) {
        std::cerr << termcolor::red << "[!] Plugin name cannot be empty" << termcolor::reset << std::endl;
        std::cerr << "Usage: mine install <name> [version] [--local] [--global]" << std::endl;
        return 1;
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
    
    // Resolve a versao dinamicamente ANTES de checar o cache local! (Suporta ^, >, <, latest)
    std::string resolved_version = version;
    bool is_semver_query = (version == "latest" || version[0] == '^' || version[0] == '>' || version[0] == '<' || version[0] == '~');
    
    if (is_semver_query) {
        std::cout << termcolor::cyan << "[*] Resolving version constraint '" << version << "' from GitHub..." << termcolor::reset << std::endl;
        std::string gh_resolved = install_resolve_github_version(registry_entry->repository, version);
        
        if (!gh_resolved.empty()) {
            resolved_version = gh_resolved; // Agora mantem o "v" (ex: "v1.1.0")
            std::cout << termcolor::green << "[*] Resolved '" << version << "' to " << resolved_version << termcolor::reset << std::endl;
        } else {
            std::cout << termcolor::red << "[!] Could not find any version matching constraint '" << version << "' on GitHub" << termcolor::reset << std::endl;
            return 1;
        }
    } else {
        // Se o usuario digitou especificamente "1.1.0", forçamos o 'v' para o resolver achar a pasta correta
        if (!resolved_version.empty() && std::isdigit(resolved_version[0])) {
            resolved_version = "v" + resolved_version;
        }
    }

    // Determine the target directory for installation
    fs::path target_plugin_dir;
    if (local_scope) {
        target_plugin_dir = get_local_plugin_dir() / plugin_name;
        std::cout << termcolor::yellow << "[*] Target: local project scope (./plugins/)" << termcolor::reset << std::endl;
    } else {
        target_plugin_dir = get_plugin_dir() / plugin_name;
        std::cout << termcolor::yellow << "[*] Target: global scope (AppData)" << termcolor::reset << std::endl;
    }
    
    // Check if already installed in the target scope
    if (local_scope ? is_plugin_installed_local(plugin_name) : is_plugin_installed(plugin_name)) {
        fs::path plugin_path = (local_scope ? get_local_plugin_dir() : get_plugin_dir()) / plugin_name / "PLUGIN.txt";
        auto meta = parse_plugin_txt(plugin_path);
        if (meta) {
            std::string clean_resolved = resolved_version;
            if (clean_resolved.size() > 0 && clean_resolved[0] == 'v') clean_resolved = clean_resolved.substr(1);
            
            std::cout << termcolor::yellow << "[*] Plugin already installed in " 
                      << (local_scope ? "local" : "global") << " scope (Current: v" << meta->version << " | Target: v" << clean_resolved << ")" << termcolor::reset << std::endl;
            
            // Pergunta antes de sobrescrever apenas se for uma versao diferente
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
    
    // --- CORRECAO PRINCIPAL ---
    // Faz o download do repo inteiro (latest) para garantir que a pasta versions/vX.X.X/ exista fisicamente no disco
    // antes do DependencyResolver tentar procura-la.
    std::cout << termcolor::cyan << "[*] Fetching repository to guarantee version folders availability..." << termcolor::reset << std::endl;
    download_and_extract_plugin(plugin_name, registry_entry->repository, "latest");
    
    // Perform installation with dependency resolution
    DependencyResolver resolver(fs::current_path(), plugin_name);
    resolver.resolve_and_install(plugin_name, resolved_version);
    
    // Write lock files for all versions
    resolver.write_lockfiles();
    
    // If local scope, copy the plugin from global to local
    if (local_scope) {
        fs::path global_plugin = get_plugin_dir() / plugin_name;
        if (fs::exists(global_plugin)) {
            std::cout << termcolor::cyan << "[*] Copying to local project scope..." << termcolor::reset << std::endl;
            
            // Remove local copy if it exists
            if (fs::exists(target_plugin_dir)) {
                fs::remove_all(target_plugin_dir);
            }
            fs::create_directories(target_plugin_dir.parent_path());
            
            // Copy the entire plugin directory
            try {
                fs::copy(global_plugin, target_plugin_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                std::cout << termcolor::green << "[+] Copied to local project scope" << termcolor::reset << std::endl;
            } catch (const std::exception& e) {
                std::cerr << termcolor::red << "[!] Failed to copy to local scope: " << e.what() << termcolor::reset << std::endl;
            }
        }
    }
    
    // Garante que o PLUGIN.txt salva a versao correta (limpa)
    fs::path target_base = local_scope ? target_plugin_dir : (get_plugin_dir() / plugin_name);
    fs::path target_plugin_txt = target_base / "PLUGIN.txt";
    if (fs::exists(target_plugin_txt)) {
        auto updated_meta = parse_plugin_txt(target_plugin_txt);
        if (updated_meta) {
            std::string save_ver = resolved_version;
            if (save_ver.size() > 0 && save_ver[0] == 'v') save_ver = save_ver.substr(1); // Tira o 'v' para salvar no arquivo
            updated_meta->version = save_ver;
            write_plugin_txt(target_plugin_txt, *updated_meta);
        }
    }
    
    // Check if the requested plugin/version is now available
    bool success = false;
    std::string clean_version = resolved_version;
    if (clean_version.size() > 0 && clean_version[0] == 'v') {
        clean_version = clean_version.substr(1);
    }
    
    // Checa com e sem o 'v' para ter certeza que localiza a instalacao
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
} // namespace mine

#endif // MINE_COMMANDS_INSTALL_HPP