#ifndef TOPAZ_COMMANDS_UPGRADE_HPP
#define TOPAZ_COMMANDS_UPGRADE_HPP

#include "core/sapphire_version.hpp"
#include "termcolor.hpp"
#include "httplib.h"
#include <iostream>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

namespace topaz {
namespace commands {

// ---------------------------------------------------------------------------
// cmd_upgrade — Baixa a versão mais recente do próprio topaz.exe
//
// O topaz.exe fica na raiz da branch "mine" do repositório Sapphire:
//   https://raw.githubusercontent.com/foxzyt/Sapphire/mine/topaz.exe
//
// Também busca versões antigas (mine.exe) para compatibilidade.
// ---------------------------------------------------------------------------
inline int cmd_upgrade() {
    namespace fs = std::filesystem;

    std::cout << termcolor::cyan
              << "[*] Checking for topaz updates..."
              << termcolor::reset << std::endl;

    // O próprio executável topaz em execução
    fs::path current_path;
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    current_path = fs::path(buffer);
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t bufsize = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &bufsize) == 0) {
        current_path = fs::path(buffer);
    } else {
        current_path = fs::path(argv[0]);
    }
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        current_path = fs::path(buffer);
    } else {
        current_path = fs::path(argv[0]);
    }
#else
    current_path = fs::path(argv[0]);
#endif

    std::string topaz_dir = current_path.parent_path().string();
    std::string topaz_name = "topaz";
#ifdef _WIN32
    topaz_name = "topaz.exe";
#endif

    std::cout << termcolor::blue
              << "[*] Current topaz path: " << current_path.string()
              << termcolor::reset << std::endl;

    // Tenta baixar o topaz.exe mais recente
    std::string url_path = std::string("/") + SAPPHIRE_REPO_OWNER
                         + "/" + SAPPHIRE_REPO_NAME
                         + "/" + SAPPHIRE_BIN_BRANCH
                         + "/" + topaz_name;

    std::cout << termcolor::cyan
              << "[*] Downloading latest " << topaz_name << " from GitHub..."
              << termcolor::reset << std::endl;

    try {
        httplib::SSLClient cli("raw.githubusercontent.com");
        cli.set_follow_location(true);
        cli.set_connection_timeout(20);
        cli.set_read_timeout(120);
        cli.enable_server_certificate_verification(false);

        auto res = cli.Get(url_path.c_str());

        if (!res) {
            std::cerr << termcolor::red
                      << "[!] Connection failed to GitHub"
                      << termcolor::reset << std::endl;
            return 1;
        }

        if (res->status == 404) {
            std::cout << termcolor::yellow
                      << "[!] No remote update found for " << topaz_name
                      << ". You may already be on the latest version."
                      << termcolor::reset << std::endl;
            return 0;
        }

        if (res->status != 200) {
            std::cerr << termcolor::red
                      << "[!] HTTP " << res->status << " fetching update"
                      << termcolor::reset << std::endl;
            return 1;
        }

        // Salva como topaz.new primeiro (não pode sobrescrever o executável em execução)
        std::string new_name = "topaz.new";
#ifdef _WIN32
        new_name = "topaz.new.exe";
#endif
        fs::path new_path = current_path.parent_path() / new_name;

        std::ofstream out(new_path, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << termcolor::red
                      << "[!] Could not write update file to " << new_path
                      << termcolor::reset << std::endl;
            return 1;
        }

        out.write(res->body.data(), static_cast<std::streamsize>(res->body.size()));
        out.close();

        uintmax_t file_size = fs::file_size(new_path);
        std::string size_str;
        if (file_size > 1024 * 1024) {
            size_str = std::to_string(file_size / (1024 * 1024)) + " MB";
        } else if (file_size > 1024) {
            size_str = std::to_string(file_size / 1024) + " KB";
        } else {
            size_str = std::to_string(file_size) + " B";
        }

        std::cout << termcolor::green
                  << "[OK] Downloaded " << size_str
                  << termcolor::reset << std::endl;

        std::cout << termcolor::yellow
                  << "[*] Topaz has been updated!"
                  << termcolor::reset << std::endl;
        std::cout << termcolor::yellow
                  << "[*] Restart topaz to use the new version."
                  << termcolor::reset << std::endl;
        std::cout << termcolor::cyan
                  << "[*] Update saved as: " << new_path.string()
                  << termcolor::reset << std::endl;
        std::cout << termcolor::yellow
                  << "[*] The new file will replace " << topaz_name << " on next restart."
                  << termcolor::reset << std::endl;

        // No Windows, podemos usar MoveFileEx com MOVEFILE_DELAY_UNTIL_REBOOT
        // para substituir na reinicialização, ou criar um script simples
#ifdef _WIN32
        // Cria um script batch para substituir o executável
        fs::path bat_path = current_path.parent_path() / "topaz_update.bat";
        std::ofstream bat(bat_path);
        bat << "@echo off\n";
        bat << "timeout /t 2 /nobreak > nul\n";
        bat << "move /y \"" << new_path.string() << "\" \"" << current_path.string() << "\" > nul\n";
        bat << "if %errorlevel%==0 (\n";
        bat << "    echo [OK] Topaz updated successfully!\n";
        bat << "    start \"\" \"" << current_path.string() << "\" upgrade --finish\n";
        bat << ") else (\n";
        bat << "    echo [ERR] Failed to replace " << topaz_name << "\n";
        bat << "    pause\n";
        bat << ")\n";
        bat << "del \"%~f0\"\n";
        bat.close();

        // Executa o script em background e sai do topaz atual
        std::cout << termcolor::green
                  << "[OK] Update script created. Restarting..."
                  << termcolor::reset << std::endl;
        std::cout << "\"" << bat_path.string() << "\"" << std::endl;

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        std::string cmd = "\"" + bat_path.string() + "\"";
        CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
#elif defined(__APPLE__)
        // macOS: Create a shell script to replace the executable
        fs::path sh_path = current_path.parent_path() / "topaz_update.sh";
        std::ofstream sh(sh_path);
        sh << "#!/bin/bash\n";
        sh << "sleep 2\n";
        sh << "mv \"" << new_path.string() << "\" \"" << current_path.string() << "\"\n";
        sh << "if [ $? -eq 0 ]; then\n";
        sh << "    echo \"[OK] Topaz updated successfully!\"\n";
        sh << "    \"" << current_path.string() << "\" upgrade --finish\n";
        sh << "else\n";
        sh << "    echo \"[ERR] Failed to replace " << topaz_name << "\"\n";
        sh << "fi\n";
        sh << "rm \"$0\"\n";
        sh.close();

        // Make script executable and run it
        std::string chmod_cmd = "chmod +x \"" + sh_path.string() + "\"";
        system(chmod_cmd.c_str());

        std::cout << termcolor::green
                  << "[OK] Update script created. Restarting..."
                  << termcolor::reset << std::endl;
        std::cout << "\"" << sh_path.string() << "\"" << std::endl;

        std::string exec_cmd = "\"" + sh_path.string() + "\" &";
        system(exec_cmd.c_str());
#elif defined(__linux__)
        // Linux: Create a shell script to replace the executable
        fs::path sh_path = current_path.parent_path() / "topaz_update.sh";
        std::ofstream sh(sh_path);
        sh << "#!/bin/bash\n";
        sh << "sleep 2\n";
        sh << "mv \"" << new_path.string() << "\" \"" << current_path.string() << "\"\n";
        sh << "if [ $? -eq 0 ]; then\n";
        sh << "    echo \"[OK] Topaz updated successfully!\"\n";
        sh << "    \"" << current_path.string() << "\" upgrade --finish\n";
        sh << "else\n";
        sh << "    echo \"[ERR] Failed to replace " << topaz_name << "\"\n";
        sh << "fi\n";
        sh << "rm \"$0\"\n";
        sh.close();

        // Make script executable and run it
        std::string chmod_cmd = "chmod +x \"" + sh_path.string() + "\"";
        system(chmod_cmd.c_str());

        std::cout << termcolor::green
                  << "[OK] Update script created. Restarting..."
                  << termcolor::reset << std::endl;
        std::cout << "\"" << sh_path.string() << "\"" << std::endl;

        std::string exec_cmd = "\"" + sh_path.string() + "\" &";
        system(exec_cmd.c_str());
#else
        std::cout << termcolor::yellow
                  << "[*] To finish the update, run:\n"
                  << "  mv \"" << new_path.string() << "\" \"" << current_path.string() << "\""
                  << termcolor::reset << std::endl;
#endif

        return 0;

    } catch (const std::exception& e) {
        std::cerr << termcolor::red
                  << "[!] Update error: " << e.what()
                  << termcolor::reset << std::endl;
        return 1;
    }
}

} // namespace commands
} // namespace topaz

#endif // TOPAZ_COMMANDS_UPGRADE_HPP