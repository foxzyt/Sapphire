#include "beryl.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include "termcolor.h"
#include "compiler.h"
#include "vm.h"
#include "utils.h"
#include "bytecode_io.h"
#include "preprocessor/preprocessor.h"
#include "lexer.h"
#include "tokens.h"
#include "config.h"

namespace fs = std::filesystem;

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}
static inline void trim(std::string &s) { rtrim(s); ltrim(s); }

BerylConfig parse_beryl_config(const std::string& path) {
    BerylConfig config;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << tc_red() << "Error: Could not open " << path << tc_reset() << std::endl;
        return config;
    }
    std::string line;
    while (std::getline(file, line)) {
        size_t cp = line.find("//");
        if (cp != std::string::npos) line = line.substr(0, cp);
        auto delim = line.find("=");
        if (delim == std::string::npos) continue;
        std::string key = line.substr(0, delim);
        std::string value = line.substr(delim + 1);
        trim(key); trim(value);
        if (!value.empty() && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.length() - 2);
        if (key == "EntryFile")      config.EntryFile = value;
        else if (key == "OutputFile")    config.OutputFile = value;
        else if (key == "Author")        config.Author = value;
        else if (key == "Version")       config.Version = value;
        else if (key == "IconPath")      config.IconPath = value;
        else if (key == "NoConsole")     config.NoConsole     = (value == "true" || value == "1");
        else if (key == "Compress")      config.Compress      = (value == "true" || value == "1");
        else if (key == "Optimize")      config.Optimize      = (value == "true" || value == "1");
        else if (key == "RequireAdmin")  config.RequireAdmin  = (value == "true" || value == "1");
        else if (key == "SoftMode")      config.SoftMode      = (value == "true" || value == "1");
        else config.CustomFields[key] = value;
    }
    if (config.OutputFile.empty()) config.OutputFile = "app.exe";
    return config;
}

bool modify_pe_subsystem(const std::string& exe_path, bool no_console) {
    std::fstream file(exe_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0x3C);
    uint32_t pe_offset;
    file.read(reinterpret_cast<char*>(&pe_offset), 4);
    file.seekg(pe_offset);
    char pe_sig[4];
    file.read(pe_sig, 4);
    if (pe_sig[0] != 'P' || pe_sig[1] != 'E' || pe_sig[2] != '\0' || pe_sig[3] != '\0') return false;
    uint32_t subsystem_offset = pe_offset + 24 + 68;
    file.seekg(subsystem_offset);
    uint16_t subsystem;
    file.read(reinterpret_cast<char*>(&subsystem), 2);
    subsystem = no_console ? 2 : 3;
    file.seekp(subsystem_offset);
    file.write(reinterpret_cast<char*>(&subsystem), 2);
    return true;
}

// Detect if the script uses UI by scanning for "UI" identifier
static bool source_uses_ui(const std::string& source) {
    Lexer lexer(source);
    for (;;) {
        Token token = lexer.scan_token();
        if (token.type == TokenType::TOKEN_IDENTIFIER && token.literal == "UI") return true;
        if (token.type == TokenType::TOKEN_END_OF_FILE) break;
    }
    return false;
}

// Read window config variables from script source
static ScriptConfig read_script_config(const std::string& source) {
    ScriptConfig config;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        size_t cp = line.find("//");
        if (cp != std::string::npos) line = line.substr(0, cp);
        auto delimPos = line.find("=");
        if (delimPos == std::string::npos) continue;
        std::string key   = line.substr(0, delimPos);
        std::string value = line.substr(delimPos + 1);
        key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
        value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
        if (key.find("config_window_width")  != std::string::npos) try { config.windowWidth  = std::stoi(value); } catch(...) {}
        else if (key.find("config_window_height") != std::string::npos) try { config.windowHeight = std::stoi(value); } catch(...) {}
        else if (key.find("config_window_title")  != std::string::npos) {
            auto sq = line.find('"'); auto eq = line.rfind('"');
            if (sq != std::string::npos && eq != sq) config.windowTitle = line.substr(sq + 1, eq - sq - 1);
        }
    }
    return config;
}

bool pack_executable(const BerylConfig& config, const std::string& runner_path) {
    std::cout << tc_cyan() << "Packing " << config.EntryFile << " -> " << config.OutputFile << tc_reset() << std::endl;

    std::string source = load_file_as_string(config.EntryFile);
    if (source.empty()) {
        std::cerr << tc_red() << "Error: Could not read entry file " << config.EntryFile << tc_reset() << std::endl;
        return false;
    }

    // Detect UI and window config BEFORE preprocessing (source still has the vars)
    bool is_ui = source_uses_ui(source);
    ScriptConfig scriptCfg = read_script_config(source);

    VM vm;
    vm.soft_mode = check_for_soft_mode(source);

    Preprocessor prep;
    source = prep.process(source);

    std::cout << "Compiling bytecode..." << std::endl;
    ObjFunction* main_function = compile(&vm, source);
    if (!main_function) {
        std::cerr << tc_red() << "Error: Compilation failed." << tc_reset() << std::endl;
        return false;
    }

    std::string temp_sbc = "temp_beryl.sbc";
    serialize_function(main_function, &vm, temp_sbc);

    std::ifstream runner_in(runner_path, std::ios::binary | std::ios::ate);
    if (!runner_in) {
        std::cerr << tc_red() << "Error: Could not find runner executable at " << runner_path << tc_reset() << std::endl;
        return false;
    }
    std::streamsize runner_size = runner_in.tellg();
    runner_in.seekg(0, std::ios::beg);

    std::ifstream sbc_in(temp_sbc, std::ios::binary | std::ios::ate);
    std::streamsize sbc_size = sbc_in.tellg();
    sbc_in.seekg(0, std::ios::beg);

    // Build BERYL_V2 metadata block:
    // [is_ui:1][window_w:4][window_h:4][title_len:4][title:N]
    std::string title    = scriptCfg.windowTitle;
    uint8_t  meta_is_ui  = is_ui ? 1 : 0;
    uint32_t meta_w      = (uint32_t)scriptCfg.windowWidth;
    uint32_t meta_h      = (uint32_t)scriptCfg.windowHeight;
    uint32_t meta_tlen   = (uint32_t)title.size();

    std::string meta_block;
    meta_block.push_back((char)meta_is_ui);
    meta_block.append(reinterpret_cast<char*>(&meta_w),    4);
    meta_block.append(reinterpret_cast<char*>(&meta_h),    4);
    meta_block.append(reinterpret_cast<char*>(&meta_tlen), 4);
    meta_block.append(title);
    uint32_t meta_size = (uint32_t)meta_block.size();

    std::ofstream out(config.OutputFile, std::ios::binary);
    if (!out) {
        std::cerr << tc_red() << "Error: Could not create output file " << config.OutputFile << tc_reset() << std::endl;
        return false;
    }

    std::cout << "Injecting VM Runner (" << runner_size / 1024 << " KB)..." << std::endl;
    out << runner_in.rdbuf();

    std::cout << "Appending Bytecode Payload (" << sbc_size << " bytes)..." << std::endl;
    out << sbc_in.rdbuf();

    // Footer layout: [meta_block][meta_size:4][payload_size:8][BERYL_V2:8]
    out.write(meta_block.data(), (std::streamsize)meta_size);
    out.write(reinterpret_cast<const char*>(&meta_size), 4);
    uint64_t payload_size = (uint64_t)sbc_size;
    out.write(reinterpret_cast<const char*>(&payload_size), 8);
    out.write("BERYL_V2", 8);

    out.close();
    runner_in.close();
    sbc_in.close();
    fs::remove(temp_sbc);

    if (config.NoConsole || is_ui) {
        std::cout << "Applying -noconsole subsystem..." << std::endl;
        if (!modify_pe_subsystem(config.OutputFile, true))
            std::cerr << tc_yellow() << "Warning: Could not modify PE subsystem." << tc_reset() << std::endl;
    }

    std::cout << tc_bold() << tc_green() << "Success! Executable created at " << config.OutputFile << tc_reset() << std::endl;
    if (is_ui) std::cout << "  [UI app: " << meta_w << "x" << meta_h << " \"" << title << "\"]" << std::endl;
    return true;
}
