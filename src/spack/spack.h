#ifndef SAPPHIRE_SPACK_H
#define SAPPHIRE_SPACK_H

#include <string>
#include <map>
#include <vector>

struct SpackConfig {
    std::string EntryFile;
    std::string OutputFile;
    std::string Author;
    std::string Version;
    std::string IconPath;
    bool NoConsole = false;
    bool Compress = false;
    bool Optimize = true;
    bool RequireAdmin = false;
    bool SoftMode = false;
    std::vector<std::string> ExtraFiles;
    std::map<std::string, std::string> CustomFields;
};

SpackConfig parse_spack_config(const std::string& path);
bool pack_executable(const SpackConfig& config, const std::string& runner_path);
void run_native_spack_ui(const std::string& runner_path);

#endif // SAPPHIRE_SPACK_H
