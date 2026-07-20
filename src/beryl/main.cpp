#include <iostream>
#include <string>
#include <filesystem>
#include "beryl.h"
#include "termcolor.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    init_terminal();

    // Resolve the true path of beryl itself (works even when called via PATH)
    fs::path build_dir;
#ifdef _WIN32
    char exe_path_buf[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path_buf, MAX_PATH) > 0) {
        build_dir = fs::path(exe_path_buf).parent_path();
    } else {
        build_dir = fs::absolute(argv[0]).parent_path();
    }
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t bufsize = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &bufsize) == 0) {
        build_dir = fs::path(buffer).parent_path();
    } else {
        build_dir = fs::absolute(argv[0]).parent_path();
    }
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        build_dir = fs::path(buffer).parent_path();
    } else {
        build_dir = fs::absolute(argv[0]).parent_path();
    }
#else
    build_dir = fs::absolute(argv[0]).parent_path();
#endif

    // Use platform-specific executable extension
    std::string exe_ext = "";
#ifdef _WIN32
    exe_ext = ".exe";
#endif
    std::string runner_path = (build_dir / ("runner" + exe_ext)).string();

    if (argc >= 2 && std::string(argv[1]) == "ui") {
        std::string sapphire_path = (build_dir / ("sapphire" + exe_ext)).string();
        std::string ui_script = (build_dir / "beryl_ui.sp").string();

        if (fs::exists(sapphire_path) && fs::exists(ui_script)) {
#ifdef _WIN32
            // cmd.exe requires the entire command to be surrounded by quotes if it contains multiple quoted strings.
            std::string cmd = "\"\"" + sapphire_path + "\" \"" + ui_script + "\"\"";
            std::system(cmd.c_str());
#else
            std::string cmd = "\"" + sapphire_path + "\" \"" + ui_script + "\"";
            std::system(cmd.c_str());
#endif
        } else {
            std::cerr << tc_red() << "Error: Could not find sapphire" + exe_ext + " or beryl_ui.sp in " << build_dir << tc_reset() << "\n";
            return 1;
        }
        return 0;
    }

    std::string config_path = "BerylConfig.txt";
    if (argc >= 3 && std::string(argv[1]) == "-c") {
        config_path = argv[2];
    } else if (argc >= 2 && std::string(argv[1]) != "-c") {
        // Simple CLI mode: `beryl script.sp`
        BerylConfig config;
        config.EntryFile = argv[1];
        config.OutputFile = fs::path(argv[1]).stem().string() + exe_ext;
        pack_executable(config, runner_path);
        return 0;
    }

    if (!fs::exists(config_path)) {
        std::cerr << tc_red() << "Error: " << config_path << " not found." << tc_reset() << "\n";
        std::cout << "Usage:\n";
        std::cout << "  beryl <script.sp>             - Fast pack without config\n";
        std::cout << "  beryl                         - Pack using BerylConfig.txt\n";
        std::cout << "  beryl -c <config.txt>         - Pack using specific config file\n";
        std::cout << "  beryl ui                      - Open Beryl graphical interface\n";
        return 1;
    }

    BerylConfig config = parse_beryl_config(config_path);
    if (config.EntryFile.empty()) {
        std::cerr << tc_red() << "Error: EntryFile is missing in " << config_path << tc_reset() << std::endl;
        return 1;
    }

    pack_executable(config, runner_path);
    return 0;
}
