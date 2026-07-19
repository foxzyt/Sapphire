#ifndef SPARK_COMMANDS_SAPPHIRE_CMD_HPP
#define SPARK_COMMANDS_SAPPHIRE_CMD_HPP

// =============================================================================
// sapphire_cmd.hpp — Comandos "spark sapphire *"
//
// Subcomandos disponíveis:
//   spark sapphire list                   — Lista versões disponíveis no GitHub
//   spark sapphire versions               — Lista versões instaladas localmente
//   spark sapphire current                — Mostra a versão ativa
//   spark sapphire install <ver>          — Instala e ativa uma versão (suporta SemVer)
//   spark sapphire use <ver>              — Ativa uma versão já instalada
//   spark sapphire uninstall <ver>        — Remove uma versão instalada
// =============================================================================

#include "core/sapphire_version.hpp"
#include "core/semver.hpp"
#include "termcolor.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>

namespace spark {
namespace commands {

// ---------------------------------------------------------------------------
// Helpers de UI
// ---------------------------------------------------------------------------

static void print_separator(char c = '-', int width = 60) {
    std::cout << std::string(width, c) << std::endl;
}

// ---------------------------------------------------------------------------
// cmd_sapphire_list — Lista versões disponíveis no repositório remoto
// ---------------------------------------------------------------------------
inline int cmd_sapphire_list() {
    std::cout << termcolor::bold << termcolor::cyan
              << "Sapphire Releases (branch: " << SAPPHIRE_BIN_BRANCH << ")"
              << termcolor::reset << std::endl;
    print_separator();

    std::cout << termcolor::yellow
              << "[*] Fetching available versions from GitHub..."
              << termcolor::reset << std::endl;

    auto remote_versions = list_remote_sapphire_versions();

    if (remote_versions.empty()) {
        std::cout << termcolor::yellow
                  << "No versions found in the 'mine' branch."
                  << termcolor::reset << std::endl;
        std::cout << "Make sure the branch exists at: https://github.com/"
                  << SAPPHIRE_REPO_OWNER << "/" << SAPPHIRE_REPO_NAME
                  << "/tree/" << SAPPHIRE_BIN_BRANCH << std::endl;
        return 1;
    }

    std::string active = read_active_version();
    auto installed     = get_installed_sapphire_versions();

    std::cout << std::endl;
    std::cout << std::left
              << std::setw(14) << "Version"
              << std::setw(14) << "Installed"
              << "Status"
              << std::endl;
    print_separator();

    // Exibe do mais novo para o mais antigo
    for (auto it = remote_versions.rbegin(); it != remote_versions.rend(); ++it) {
        const std::string& ver = *it;

        bool is_installed = std::find(installed.begin(), installed.end(), ver) != installed.end();
        bool is_active    = (ver == semver::normalize(active));

        std::string ver_str = semver::with_v(ver);
        std::string inst_str = is_installed ? "yes" : "-";

        if (is_active) {
            std::cout << termcolor::green << termcolor::bold;
            std::cout << std::left
                      << std::setw(14) << ver_str
                      << std::setw(14) << inst_str
                      << "<-- active"
                      << termcolor::reset << std::endl;
        } else if (is_installed) {
            std::cout << termcolor::cyan;
            std::cout << std::left
                      << std::setw(14) << ver_str
                      << std::setw(14) << inst_str
                      << "installed"
                      << termcolor::reset << std::endl;
        } else {
            std::cout << std::left
                      << std::setw(14) << ver_str
                      << std::setw(14) << inst_str
                      << ""
                      << std::endl;
        }
    }

    print_separator();
    std::cout << termcolor::bold
              << "Total: " << remote_versions.size() << " version(s) available"
              << termcolor::reset << std::endl;

    if (!active.empty()) {
        std::cout << termcolor::green
                  << "Active: " << semver::with_v(active)
                  << termcolor::reset << std::endl;
    } else {
        std::cout << termcolor::yellow
                  << "Active: (none — run: spark sapphire install latest)"
                  << termcolor::reset << std::endl;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// cmd_sapphire_versions — Lista versões instaladas localmente
// ---------------------------------------------------------------------------
inline int cmd_sapphire_versions() {
    std::cout << termcolor::bold << "Sapphire — Installed Versions"
              << termcolor::reset << std::endl;
    print_separator();

    auto installed = get_installed_sapphire_versions();
    std::string active = read_active_version();

    if (installed.empty()) {
        std::cout << termcolor::yellow
                  << "(No versions installed)"
                  << termcolor::reset << std::endl;
        std::cout << "Run: " << termcolor::green
                  << "spark sapphire install latest"
                  << termcolor::reset << std::endl;
        return 0;
    }

    fs::path bin_dir = get_sapphire_bin_dir();

    for (auto it = installed.rbegin(); it != installed.rend(); ++it) {
        const std::string& ver = *it;
        bool is_active = (semver::normalize(ver) == semver::normalize(active));
        fs::path ver_dir = get_sapphire_version_dir(ver);

        // Calcula tamanho total dos binários
        uintmax_t total_size = 0;
        try {
            for (const auto& bin : SAPPHIRE_BINARIES) {
                fs::path bin_path = ver_dir / bin;
                if (fs::exists(bin_path)) {
                    total_size += fs::file_size(bin_path);
                }
            }
        } catch (...) {}

        // Converte para MB
        std::string size_str;
        if (total_size > 1024 * 1024) {
            size_str = std::to_string(total_size / (1024 * 1024)) + " MB";
        } else if (total_size > 1024) {
            size_str = std::to_string(total_size / 1024) + " KB";
        } else {
            size_str = std::to_string(total_size) + " B";
        }

        if (is_active) {
            std::cout << termcolor::green << termcolor::bold
                      << "  * " << semver::with_v(ver)
                      << termcolor::reset;
            std::cout << "  (" << size_str << ")  <-- active" << std::endl;
        } else {
            std::cout << "    " << semver::with_v(ver)
                      << "  (" << size_str << ")" << std::endl;
        }
    }

    print_separator();
    std::cout << "Location: " << termcolor::cyan
              << bin_dir.string()
              << termcolor::reset << std::endl;

    return 0;
}

// ---------------------------------------------------------------------------
// cmd_sapphire_current — Mostra a versão ativa
// ---------------------------------------------------------------------------
inline int cmd_sapphire_current() {
    std::string active = read_active_version();

    if (active.empty()) {
        std::cout << termcolor::yellow
                  << "No active Sapphire version."
                  << termcolor::reset << std::endl;
        std::cout << "Run: " << termcolor::green
                  << "spark sapphire install latest"
                  << termcolor::reset << std::endl;
        return 1;
    }

    std::cout << termcolor::green << termcolor::bold
              << semver::with_v(active)
              << termcolor::reset << std::endl;

    fs::path bin_dir = get_sapphire_bin_dir();
    std::cout << "Location: " << bin_dir.string() << std::endl;

    // Lista binários da versão ativa
    std::cout << "Binaries:" << std::endl;
    for (const auto& bin : SAPPHIRE_BINARIES) {
        fs::path bin_path = bin_dir / bin;
        if (fs::exists(bin_path)) {
            uintmax_t sz = 0;
            try { sz = fs::file_size(bin_path); } catch (...) {}
            std::string sz_str;
            if (sz > 1024 * 1024) {
                sz_str = std::to_string(sz / (1024 * 1024)) + " MB";
            } else {
                sz_str = std::to_string(sz / 1024) + " KB";
            }
            std::cout << "  " << termcolor::green << "[+] " << termcolor::reset
                      << std::left << std::setw(18) << bin << sz_str << std::endl;
        } else {
            std::cout << "  " << termcolor::yellow << "[-] " << termcolor::reset
                      << bin << " (missing)" << std::endl;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// cmd_sapphire_install — Instala (e ativa) uma versão da Sapphire
//   Suporta SemVer: "latest", "1.0.6", "^1.0", ">=1.0.5", "<2.0"
// ---------------------------------------------------------------------------
inline int cmd_sapphire_install(const std::string& constraint) {
    std::cout << termcolor::cyan
              << "[*] Resolving version constraint: '"
              << (constraint.empty() ? "latest" : constraint) << "'"
              << termcolor::reset << std::endl;

    std::string effective_constraint = constraint.empty() ? "latest" : constraint;

    // Para "latest" ou constraints SemVer, resolve contra o remoto
    bool needs_remote_resolve = (effective_constraint == "latest")
        || (!effective_constraint.empty() && (
            effective_constraint[0] == '^' ||
            effective_constraint[0] == '~' ||
            effective_constraint[0] == '>' ||
            effective_constraint[0] == '<' ||
            effective_constraint[0] == '='));

    std::string resolved_version;

    if (needs_remote_resolve) {
        std::cout << termcolor::yellow
                  << "[*] Fetching available versions from GitHub..."
                  << termcolor::reset << std::endl;

        auto remote_versions = list_remote_sapphire_versions();

        if (remote_versions.empty()) {
            std::cerr << termcolor::red
                      << "[!] Could not fetch version list from GitHub."
                      << termcolor::reset << std::endl;
            return 1;
        }

        resolved_version = semver::resolve_best(remote_versions, effective_constraint);

        if (resolved_version.empty()) {
            std::cerr << termcolor::red
                      << "[!] No version satisfies constraint '" << effective_constraint << "'"
                      << termcolor::reset << std::endl;
            std::cout << "Available: ";
            for (auto it = remote_versions.rbegin(); it != remote_versions.rend(); ++it) {
                std::cout << semver::with_v(*it) << " ";
            }
            std::cout << std::endl;
            return 1;
        }

        std::cout << termcolor::green
                  << "[*] Resolved '" << effective_constraint
                  << "' -> " << semver::with_v(resolved_version)
                  << termcolor::reset << std::endl;
    } else {
        // Versão específica fornecida diretamente: "1.0.6" ou "v1.0.6"
        resolved_version = semver::normalize(effective_constraint);
    }

    // Verifica se já está instalada
    if (is_sapphire_version_installed(resolved_version)) {
        std::string active = read_active_version();

        std::cout << termcolor::yellow
                  << "[*] Sapphire " << semver::with_v(resolved_version)
                  << " is already installed."
                  << termcolor::reset << std::endl;

        if (semver::normalize(active) != resolved_version) {
            std::cout << "Activate it? (Y/n): ";
            std::string resp;
            std::getline(std::cin, resp);
            while (!resp.empty() && (resp.back() == '\r' || resp.back() == '\n')) {
                resp.pop_back();
            }
            if (resp != "n" && resp != "N") {
                return activate_sapphire_version(resolved_version) ? 0 : 1;
            }
        } else {
            std::cout << termcolor::green
                      << "[*] " << semver::with_v(resolved_version)
                      << " is already the active version."
                      << termcolor::reset << std::endl;
        }
        return 0;
    }

    // Faz o download e ativa
    if (!install_sapphire_version(resolved_version, true)) {
        return 1;
    }

    std::cout << std::endl;
    std::cout << termcolor::yellow << termcolor::bold
              << "Make sure " << get_sapphire_bin_dir().string()
              << " is in your PATH to use the sapphire commands."
              << termcolor::reset << std::endl;

    return 0;
}

// ---------------------------------------------------------------------------
// cmd_sapphire_use — Ativa uma versão já instalada
// ---------------------------------------------------------------------------
inline int cmd_sapphire_use(const std::string& version) {
    if (version.empty()) {
        std::cerr << termcolor::red
                  << "[!] Missing version argument"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: spark sapphire use <version>" << std::endl;
        std::cerr << "Installed versions: ";
        auto installed = get_installed_sapphire_versions();
        for (const auto& v : installed) std::cerr << semver::with_v(v) << " ";
        std::cerr << std::endl;
        return 1;
    }

    std::string clean = semver::normalize(version);

    if (!is_sapphire_version_installed(clean)) {
        std::cerr << termcolor::red
                  << "[!] Version " << semver::with_v(clean) << " is not installed."
                  << termcolor::reset << std::endl;
        std::cout << "Run: " << termcolor::green
                  << "spark sapphire install " << clean
                  << termcolor::reset << std::endl;

        auto installed = get_installed_sapphire_versions();
        if (!installed.empty()) {
            std::cout << "Installed: ";
            for (const auto& v : installed) std::cout << semver::with_v(v) << " ";
            std::cout << std::endl;
        }
        return 1;
    }

    return activate_sapphire_version(clean) ? 0 : 1;
}

// ---------------------------------------------------------------------------
// cmd_sapphire_uninstall — Remove uma versão instalada
// ---------------------------------------------------------------------------
inline int cmd_sapphire_uninstall(const std::string& version) {
    if (version.empty()) {
        std::cerr << termcolor::red
                  << "[!] Missing version argument"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: spark sapphire uninstall <version>" << std::endl;
        return 1;
    }

    std::string clean  = semver::normalize(version);
    std::string active = read_active_version();
    fs::path ver_dir   = get_sapphire_version_dir(clean);

    if (!fs::exists(ver_dir)) {
        std::cout << termcolor::yellow
                  << "[*] Sapphire " << semver::with_v(clean)
                  << " is not installed."
                  << termcolor::reset << std::endl;
        return 0;
    }

    // Avisa se tentar remover a versão ativa
    if (semver::normalize(active) == clean) {
        std::cout << termcolor::yellow
                  << "[!] WARNING: " << semver::with_v(clean)
                  << " is currently the active version."
                  << termcolor::reset << std::endl;
        std::cout << "Uninstall anyway? (y/N): ";
        std::string resp;
        std::getline(std::cin, resp);
        while (!resp.empty() && (resp.back() == '\r' || resp.back() == '\n')) {
            resp.pop_back();
        }
        if (resp != "y" && resp != "Y") {
            std::cout << termcolor::cyan
                      << "[*] Uninstall cancelled."
                      << termcolor::reset << std::endl;
            return 0;
        }
    }

    try {
        fs::remove_all(ver_dir);
        std::cout << termcolor::green
                  << "[OK] Removed Sapphire " << semver::with_v(clean)
                  << termcolor::reset << std::endl;

        // Se era a versão ativa, limpa o .version
        if (semver::normalize(active) == clean) {
            // Tenta ativar a versão mais recente restante
            auto remaining = get_installed_sapphire_versions();
            if (!remaining.empty()) {
                std::string newest = remaining.back();
                std::cout << termcolor::yellow
                          << "[*] Switching active to " << semver::with_v(newest) << "..."
                          << termcolor::reset << std::endl;
                activate_sapphire_version(newest);
            } else {
                // Remove o arquivo .version
                try { fs::remove(get_active_version_file()); } catch (...) {}
                std::cout << termcolor::yellow
                          << "[*] No other versions installed. Active version cleared."
                          << termcolor::reset << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << termcolor::red
                  << "[!] Failed to remove version: " << e.what()
                  << termcolor::reset << std::endl;
        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// print_sapphire_help — Exibe ajuda do subsistema sapphire
// ---------------------------------------------------------------------------
inline void print_sapphire_help() {
    std::cout << termcolor::bold << termcolor::cyan
              << "spark sapphire" << termcolor::reset
              << " — Sapphire Runtime Version Manager"
              << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::bold << "Commands:" << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::green << "spark sapphire list"
              << termcolor::reset
              << "                  List all available versions (remote)" << std::endl;
    std::cout << "  " << termcolor::green << "spark sapphire versions"
              << termcolor::reset
              << "               List installed versions (local)" << std::endl;
    std::cout << "  " << termcolor::green << "spark sapphire current"
              << termcolor::reset
              << "                Show the currently active version" << std::endl;
    std::cout << "  " << termcolor::green << "spark sapphire install <version>"
              << termcolor::reset
              << "      Install (and activate) a version" << std::endl;
    std::cout << "  " << termcolor::green << "spark sapphire use <version>"
              << termcolor::reset
              << "          Activate an already-installed version" << std::endl;
    std::cout << "  " << termcolor::green << "spark sapphire uninstall <version>"
              << termcolor::reset
              << "    Remove an installed version" << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::magenta << "Version Constraints (SemVer):" << termcolor::reset << std::endl;
    std::cout << "  " << termcolor::cyan << "latest"    << termcolor::reset << "          Resolves to the newest available version" << std::endl;
    std::cout << "  " << termcolor::cyan << "1.0.9"     << termcolor::reset << "           Exact version match" << std::endl;
    std::cout << "  " << termcolor::cyan << "\"^1.0\""   << termcolor::reset << "           Compatible with 1.x (same major)" << std::endl;
    std::cout << "  " << termcolor::cyan << "\">1.0.9\"" << termcolor::reset << "          Greater than 1.0.9" << std::endl;
    std::cout << "  " << termcolor::cyan << "\">=1.0.9\"" << termcolor::reset << "         Greater than or equal to 1.0.9" << std::endl;
    std::cout << termcolor::red
              << "  * Use quotes for constraints with >, <, ^ to avoid terminal redirection!"
              << termcolor::reset << std::endl;
    std::cout << std::endl;

    std::cout << termcolor::yellow
              << "Binaries installed: sapphire.exe, runner.exe, beryl.exe"
              << termcolor::reset << std::endl;
    std::cout << termcolor::yellow
              << "Install path: " << get_sapphire_bin_dir().string()
              << termcolor::reset << std::endl;
}

// ---------------------------------------------------------------------------
// cmd_sapphire_dispatch — Ponto de entrada principal para "spark sapphire ..."
// ---------------------------------------------------------------------------
inline int cmd_sapphire_dispatch(int argc, char* argv[], int sapphire_arg_offset) {
    // sapphire_arg_offset = índice de argv onde começa o subcomando
    // Ex: "spark sapphire install 1.0.9"  → argv[2]="install", argv[3]="1.0.9"

    if (sapphire_arg_offset >= argc) {
        print_sapphire_help();
        return 0;
    }

    std::string subcmd = argv[sapphire_arg_offset];

    if (subcmd == "list") {
        return cmd_sapphire_list();
    }
    else if (subcmd == "versions") {
        return cmd_sapphire_versions();
    }
    else if (subcmd == "current") {
        return cmd_sapphire_current();
    }
    else if (subcmd == "install") {
        std::string ver;
        if (sapphire_arg_offset + 1 < argc) {
            ver = argv[sapphire_arg_offset + 1];
        }
        return cmd_sapphire_install(ver);
    }
    else if (subcmd == "use") {
        std::string ver;
        if (sapphire_arg_offset + 1 < argc) {
            ver = argv[sapphire_arg_offset + 1];
        }
        return cmd_sapphire_use(ver);
    }
    else if (subcmd == "uninstall" || subcmd == "remove") {
        std::string ver;
        if (sapphire_arg_offset + 1 < argc) {
            ver = argv[sapphire_arg_offset + 1];
        }
        return cmd_sapphire_uninstall(ver);
    }
    else if (subcmd == "help" || subcmd == "--help" || subcmd == "-h") {
        print_sapphire_help();
        return 0;
    }
    else {
        std::cerr << termcolor::red
                  << "[!] Unknown sapphire subcommand: '" << subcmd << "'"
                  << termcolor::reset << std::endl;
        std::cerr << "Run 'spark sapphire help' for usage" << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace spark

#endif // SPARK_COMMANDS_SAPPHIRE_CMD_HPP
