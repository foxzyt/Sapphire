#ifndef SAPPHIRE_BERYL_H
#define SAPPHIRE_BERYL_H

#include <string>
#include <map>
#include <vector>

struct BerylConfig {
    std::string EntryFile;
    std::string OutputFile;
    std::string Author;
    std::string Version;
    std::string IconPath;
    std::string AssetsFolder;
    bool NoConsole = false;
    bool Compress = false;
    bool Encrypt = false;
    bool Optimize = true;
    bool RequireAdmin = false;
    bool SoftMode = false;
    std::vector<std::string> ExtraFiles;
    std::map<std::string, std::string> CustomFields;
};

BerylConfig parse_beryl_config(const std::string& path);
bool pack_executable(const BerylConfig& config, const std::string& runner_path);
void run_native_beryl_ui(const std::string& runner_path);

#endif // SAPPHIRE_BERYL_H
