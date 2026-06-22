#include <iostream>
#include <string>
#include <filesystem>
#include "spack.h"
#include "termcolor.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    init_terminal();

    // Resolve the true path of spack.exe itself (works even when called via PATH)
    fs::path build_dir;
#ifdef _WIN32
    char exe_path_buf[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path_buf, MAX_PATH) > 0) {
        build_dir = fs::path(exe_path_buf).parent_path();
    } else {
        build_dir = fs::absolute(argv[0]).parent_path();
    }
#else
    try {
        build_dir = fs::canonical(fs::absolute(argv[0])).parent_path();
    } catch (...) {
        build_dir = fs::absolute(argv[0]).parent_path();
    }
#endif
    std::string runner_path = (build_dir / "runner.exe").string();

    if (argc >= 2 && std::string(argv[1]) == "ui") {
        std::string sapphire_path = (build_dir / "sapphire.exe").string();
        std::string ui_script = (build_dir / "spack_ui.sp").string();
        
        if (fs::exists(sapphire_path) && fs::exists(ui_script)) {
            // cmd.exe requires the entire command to be surrounded by quotes if it contains multiple quoted strings.
            std::string cmd = "\"\"" + sapphire_path + "\" \"" + ui_script + "\"\"";
            std::system(cmd.c_str());
        } else {
            std::cerr << tc_red() << "Error: Could not find sapphire.exe or spack_ui.sp in " << build_dir << tc_reset() << "\n";
            return 1;
        }
        return 0;
    }

    std::string config_path = "SpackConfig.txt";
    if (argc >= 3 && std::string(argv[1]) == "-c") {
        config_path = argv[2];
    } else if (argc >= 2 && std::string(argv[1]) != "-c") {
        // Simple CLI mode: `spack script.sp`
        SpackConfig config;
        config.EntryFile = argv[1];
        config.OutputFile = fs::path(argv[1]).stem().string() + ".exe";
        pack_executable(config, runner_path);
        return 0;
    }

    if (!fs::exists(config_path)) {
        std::cerr << tc_red() << "Error: " << config_path << " not found." << tc_reset() << "\n";
        std::cout << "Usage:\n";
        std::cout << "  spack <script.sp>             - Fast pack without config\n";
        std::cout << "  spack                         - Pack using SpackConfig.txt\n";
        std::cout << "  spack -c <config.txt>         - Pack using specific config file\n";
        std::cout << "  spack ui                      - Open Spack graphical interface\n";
        return 1;
    }

    SpackConfig config = parse_spack_config(config_path);
    if (config.EntryFile.empty()) {
        std::cerr << tc_red() << "Error: EntryFile is missing in " << config_path << tc_reset() << std::endl;
        return 1;
    }

    pack_executable(config, runner_path);
    return 0;
}
