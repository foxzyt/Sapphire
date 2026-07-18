#ifndef SPARK_CORE_SAPPHIRE_VERSION_HPP
#define SPARK_CORE_SAPPHIRE_VERSION_HPP

// =============================================================================
// sapphire_version.hpp — Gerenciamento de versões dos binários da Sapphire
//
// Consome a branch "mine" do repositório foxzyt/Sapphire no GitHub.
// Estrutura esperada na branch:
//
//   v1.0.9/
//     sapphire.exe
//     runner.exe
//     spack.exe
//     spark.exe
//   v1.1.0/
//     ...
//
// Binários instalados em: %APPDATA%\Sapphire\bin\<version>\
// Versão ativa marcada em: %APPDATA%\Sapphire\bin\.version
// =============================================================================

#include "fs_utils.hpp"
#include "semver.hpp"
#include "termcolor.hpp"
#include "httplib.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>

namespace spark {

namespace fs = std::filesystem;

// Binários presentes em cada release
// NOTA: spark.exe (o package manager) NÃO está aqui - ele é gerenciado separadamente
// via "spark upgrade", pois o usuário já tem o spark para usar este comando.
static const std::vector<std::string> SAPPHIRE_BINARIES = {
    "sapphire.exe",
    "runner.exe",
    "spack.exe"
};

// Repositório e branch dos binários
constexpr const char* SAPPHIRE_REPO_OWNER = "foxzyt";
constexpr const char* SAPPHIRE_REPO_NAME  = "Sapphire";
constexpr const char* SAPPHIRE_BIN_BRANCH = "mine";

// ---------------------------------------------------------------------------
// get_sapphire_bin_dir — Diretório base dos binários instalados
//   Windows: %APPDATA%\Sapphire\bin\
//   Linux:   ~/.local/share/Sapphire/bin/
// ---------------------------------------------------------------------------
inline fs::path get_sapphire_bin_dir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) {
        throw std::runtime_error("Could not access APPDATA environment variable");
    }
    fs::path bin_dir = fs::path(appdata) / "Sapphire" / "bin";
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        throw std::runtime_error("Could not access HOME environment variable");
    }
    fs::path bin_dir = fs::path(home) / ".local" / "share" / "Sapphire" / "bin";
#endif
    fs::create_directories(bin_dir);
    return bin_dir;
}

// ---------------------------------------------------------------------------
// get_active_version_file — Caminho do arquivo que registra a versão ativa
// ---------------------------------------------------------------------------
inline fs::path get_active_version_file() {
    return get_sapphire_bin_dir() / ".version";
}

// ---------------------------------------------------------------------------
// get_sapphire_version_dir — Diretório de uma versão específica
// ---------------------------------------------------------------------------
inline fs::path get_sapphire_version_dir(const std::string& version) {
    return get_sapphire_bin_dir() / semver::with_v(version);
}

// ---------------------------------------------------------------------------
// read_active_version — Lê a versão ativa do arquivo .version
//   Retorna "" se nenhuma versão estiver ativa
// ---------------------------------------------------------------------------
inline std::string read_active_version() {
    fs::path vfile = get_active_version_file();
    if (!fs::exists(vfile)) return "";

    std::ifstream f(vfile);
    if (!f.is_open()) return "";

    std::string ver;
    std::getline(f, ver);

    // Remove espaços/newlines residuais
    while (!ver.empty() && (ver.back() == '\r' || ver.back() == '\n' || ver.back() == ' ')) {
        ver.pop_back();
    }

    return ver;
}

// ---------------------------------------------------------------------------
// write_active_version — Grava a versão ativa no arquivo .version
// ---------------------------------------------------------------------------
inline bool write_active_version(const std::string& version) {
    fs::path vfile = get_active_version_file();
    std::ofstream f(vfile);
    if (!f.is_open()) return false;
    f << semver::normalize(version) << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// get_installed_sapphire_versions — Lista todas as versões instaladas
//   (diretórios v* dentro do bin_dir)
// ---------------------------------------------------------------------------
inline std::vector<std::string> get_installed_sapphire_versions() {
    std::vector<std::string> versions;
    fs::path bin_dir = get_sapphire_bin_dir();

    if (!fs::exists(bin_dir)) return versions;

    for (const auto& entry : fs::directory_iterator(bin_dir)) {
        if (!fs::is_directory(entry.path())) continue;
        std::string name = entry.path().filename().string();
        if (name.size() > 1 && name[0] == 'v') {
            versions.push_back(semver::normalize(name)); // sem 'v'
        }
    }

    // Ordena por SemVer (menor primeiro)
    std::sort(versions.begin(), versions.end(), [](const std::string& a, const std::string& b) {
        return semver::compare(a, b) < 0;
    });

    return versions;
}

// ---------------------------------------------------------------------------
// is_sapphire_version_installed — Verifica se uma versão já está instalada
// ---------------------------------------------------------------------------
inline bool is_sapphire_version_installed(const std::string& version) {
    fs::path vdir = get_sapphire_version_dir(version);
    if (!fs::exists(vdir)) return false;

    // Verifica se pelo menos o sapphire.exe está presente
    return fs::exists(vdir / "sapphire.exe") || fs::exists(vdir / "sapphire");
}

// ---------------------------------------------------------------------------
// list_remote_sapphire_versions — Consulta a GitHub Contents API para listar
//   versões disponíveis na branch SAPPHIRE_BIN_BRANCH do repositório.
//   Retorna vetor de versões limpas (ex: "1.0.6", "1.0.7") ordenadas.
// ---------------------------------------------------------------------------
inline std::vector<std::string> list_remote_sapphire_versions() {
    std::vector<std::string> versions;

    try {
        httplib::SSLClient cli("api.github.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(15);
        cli.set_read_timeout(20);
        cli.enable_server_certificate_verification(false);

        httplib::Headers headers = {
            {"User-Agent", "Sapphire-Spark/2.2"},
            {"Accept", "application/vnd.github.v3+json"}
        };

        // GET /repos/foxzyt/Sapphire/contents?ref=mine
        std::string path = std::string("/repos/") + SAPPHIRE_REPO_OWNER
                         + "/" + SAPPHIRE_REPO_NAME
                         + "/contents?ref=" + SAPPHIRE_BIN_BRANCH;

        auto res = cli.Get(path.c_str(), headers);

        if (!res) {
            std::cerr << termcolor::red
                      << "[!] Connection failed to api.github.com"
                      << termcolor::reset << std::endl;
            return versions;
        }

        if (res->status != 200) {
            std::cerr << termcolor::red
                      << "[!] GitHub API returned HTTP " << res->status
                      << " — check if the branch '" << SAPPHIRE_BIN_BRANCH << "' exists"
                      << termcolor::reset << std::endl;
            return versions;
        }

        nlohmann::json items = nlohmann::json::parse(res->body);

        if (!items.is_array()) {
            std::cerr << termcolor::red
                      << "[!] Unexpected response from GitHub API"
                      << termcolor::reset << std::endl;
            return versions;
        }

        for (const auto& item : items) {
            if (!item.contains("type") || !item.contains("name")) continue;
            if (item["type"] != "dir") continue;

            std::string name = item["name"].get<std::string>();

            // Apenas diretórios que começam com 'v' seguido de dígito
            if (name.size() >= 2 && name[0] == 'v' && std::isdigit(static_cast<unsigned char>(name[1]))) {
                versions.push_back(semver::normalize(name)); // sem 'v'
            }
        }

    } catch (const nlohmann::json::exception& e) {
        std::cerr << termcolor::red
                  << "[!] JSON parse error: " << e.what()
                  << termcolor::reset << std::endl;
    } catch (const std::exception& e) {
        std::cerr << termcolor::red
                  << "[!] Error listing remote versions: " << e.what()
                  << termcolor::reset << std::endl;
    }

    // Ordena por SemVer
    std::sort(versions.begin(), versions.end(), [](const std::string& a, const std::string& b) {
        return semver::compare(a, b) < 0;
    });

    return versions;
}

// ---------------------------------------------------------------------------
// download_sapphire_binary — Baixa um único binário de uma release
//   URL: https://raw.githubusercontent.com/foxzyt/Sapphire/mine/v1.0.7/sapphire.exe
// ---------------------------------------------------------------------------
inline bool download_sapphire_binary(
    const std::string& version,   // clean version ex: "1.0.7"
    const std::string& filename,  // ex: "sapphire.exe"
    const fs::path& dest_dir
) {
    std::string raw_version = semver::with_v(version); // "v1.0.7"

    std::string url_path = std::string("/") + SAPPHIRE_REPO_OWNER
                         + "/" + SAPPHIRE_REPO_NAME
                         + "/" + SAPPHIRE_BIN_BRANCH
                         + "/" + raw_version
                         + "/" + filename;

    try {
        httplib::SSLClient cli("raw.githubusercontent.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(20);
        cli.set_read_timeout(120); // Binários podem ser grandes
        cli.enable_server_certificate_verification(false);

        auto res = cli.Get(url_path.c_str());

        if (!res) {
            std::cerr << termcolor::red
                      << "[!] Connection failed while downloading " << filename
                      << termcolor::reset << std::endl;
            return false;
        }

        if (res->status == 404) {
            std::cerr << termcolor::yellow
                      << "[!] Binary not found: " << filename
                      << " (version " << raw_version << ")"
                      << termcolor::reset << std::endl;
            return false;
        }

        if (res->status != 200) {
            std::cerr << termcolor::red
                      << "[!] HTTP " << res->status << " downloading " << filename
                      << termcolor::reset << std::endl;
            return false;
        }

        fs::create_directories(dest_dir);
        fs::path out_path = dest_dir / filename;

        std::ofstream out(out_path, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << termcolor::red
                      << "[!] Could not write file: " << out_path
                      << termcolor::reset << std::endl;
            return false;
        }

        out.write(res->body.data(), static_cast<std::streamsize>(res->body.size()));
        out.close();

        // No Linux/macOS: torna o arquivo executável (chmod +x)
#ifndef _WIN32
        try {
            fs::permissions(out_path,
                fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                fs::perm_options::add);
        } catch (...) {}
#endif

        return true;

    } catch (const std::exception& e) {
        std::cerr << termcolor::red
                  << "[!] Download error for " << filename << ": " << e.what()
                  << termcolor::reset << std::endl;
        return false;
    }
}

// ---------------------------------------------------------------------------
// activate_sapphire_version — Copia os binários de <bin_dir>/<version>/ para
//   o diretório raiz <bin_dir>/ (versão "ativa"), sobrescrevendo arquivos.
//   Também atualiza o arquivo .version.
// ---------------------------------------------------------------------------
inline bool activate_sapphire_version(const std::string& version) {
    std::string clean = semver::normalize(version);
    fs::path ver_dir  = get_sapphire_version_dir(clean);
    fs::path bin_dir  = get_sapphire_bin_dir();

    if (!fs::exists(ver_dir)) {
        std::cerr << termcolor::red
                  << "[!] Version " << semver::with_v(clean)
                  << " is not installed. Run: spark sapphire install " << clean
                  << termcolor::reset << std::endl;
        return false;
    }

    std::cout << termcolor::cyan
              << "[*] Activating Sapphire " << semver::with_v(clean) << "..."
              << termcolor::reset << std::endl;

    bool all_ok = true;
    for (const auto& bin : SAPPHIRE_BINARIES) {
        fs::path src  = ver_dir / bin;
        fs::path dest = bin_dir / bin;

        if (!fs::exists(src)) {
            // Arquivo pode não existir nesta versão — não é erro fatal
            continue;
        }

        try {
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
            std::cout << "  " << termcolor::green << "[+] " << termcolor::reset
                      << bin << std::endl;
        } catch (const std::exception& e) {
            std::cerr << termcolor::red
                      << "[!] Failed to copy " << bin << ": " << e.what()
                      << termcolor::reset << std::endl;
            all_ok = false;
        }
    }

    if (all_ok) {
        write_active_version(clean);
        std::cout << termcolor::green << termcolor::bold
                  << "[OK] Sapphire " << semver::with_v(clean) << " is now active."
                  << termcolor::reset << std::endl;
    }

    return all_ok;
}

// ---------------------------------------------------------------------------
// install_sapphire_version — Baixa todos os binários de uma versão e ativa.
//   Se `activate` for true, também ativa após o download.
// ---------------------------------------------------------------------------
inline bool install_sapphire_version(const std::string& version, bool activate = true) {
    std::string clean = semver::normalize(version);
    fs::path ver_dir  = get_sapphire_version_dir(clean);

    std::cout << termcolor::cyan
              << "[*] Downloading Sapphire " << semver::with_v(clean) << "..."
              << termcolor::reset << std::endl;

    int downloaded = 0;
    int failed = 0;

    for (const auto& bin : SAPPHIRE_BINARIES) {
        std::cout << "  -> " << bin << "... ";
        std::cout.flush();

        if (download_sapphire_binary(clean, bin, ver_dir)) {
            std::cout << termcolor::green << "OK" << termcolor::reset << std::endl;
            downloaded++;
        } else {
            std::cout << termcolor::yellow << "SKIP" << termcolor::reset << std::endl;
            failed++;
        }
    }

    if (downloaded == 0) {
        std::cerr << termcolor::red
                  << "[!] No binaries downloaded for version " << semver::with_v(clean)
                  << ". Check if the version exists in the 'mine' branch."
                  << termcolor::reset << std::endl;
        // Limpa diretório vazio
        try { fs::remove_all(ver_dir); } catch (...) {}
        return false;
    }

    std::cout << termcolor::green
              << "[*] Downloaded " << downloaded << " binary(ies)"
              << (failed > 0 ? " (" + std::to_string(failed) + " skipped)" : "")
              << termcolor::reset << std::endl;

    if (activate) {
        return activate_sapphire_version(clean);
    }

    return true;
}

} // namespace spark

#endif // SPARK_CORE_SAPPHIRE_VERSION_HPP
