#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include <vector>
#include <filesystem>
#include <random>
#include "vm.h"
#include "object.h"
#include "utils.h"
#include "termcolor.h"
#include "value.h"
#include "bytecode_io.h"
#include "tokens.h"
#include "config.h"
#include <SFML/Graphics.hpp>
#include "../spark/third_party/miniz.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

ObjFunction* deserialize_function(VM* vm, const std::string& path);
ObjFunction* deserialize_function_from_stream(VM* vm, std::istream& in);

std::string get_executable_path() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return std::string(buffer);
#else
    return "";
#endif
}

struct BerylMeta {
    bool     is_ui       = false;
    uint32_t window_w    = 800;
    uint32_t window_h    = 600;
    std::string title    = "Sapphire App";
    uint8_t encryption_key[32] = {0};
    bool has_encryption  = false;
};

std::string g_temp_assets_dir = "";

void cleanup_temp_assets() {
    if (!g_temp_assets_dir.empty()) {
        try {
            fs::current_path(fs::temp_directory_path());
            fs::remove_all(g_temp_assets_dir);
        } catch (...) {}
    }
}

bool extract_assets_zip(const std::vector<uint8_t>& zip_data, const std::string& target_dir) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_mem(&zip_archive, zip_data.data(), zip_data.size(), 0)) {
        return false;
    }

    fs::create_directories(target_dir);
    int num_files = (int)mz_zip_reader_get_num_files(&zip_archive);
    
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
        
        fs::path out_path = fs::path(target_dir) / file_stat.m_filename;
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            fs::create_directories(out_path);
        } else {
            fs::create_directories(out_path.parent_path());
            mz_zip_reader_extract_to_file(&zip_archive, i, out_path.string().c_str(), 0);
        }
    }
    
    mz_zip_reader_end(&zip_archive);
    return true;
}

std::string generate_random_dir_name() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    const char* hex = "0123456789abcdef";
    std::string name = "sapphire_app_";
    for(int i = 0; i < 16; i++) name += hex[dis(gen)];
    return name;
}

// Parse BERYL_V3 or V2 metadata.
bool parse_beryl_footer(std::ifstream& exe_file, std::streamsize file_size,
                        ObjFunction** out_function, VM* vm, BerylMeta& meta) {
    if (file_size < 28) return false;

    exe_file.seekg(-8, std::ios::end);
    char magic[9] = {0};
    exe_file.read(magic, 8);
    std::string magic_str(magic, 8);

    if (magic_str == "BERYL_V3") {
        // Footer: [meta][meta_size:4][payload_size:8][assets_size:8][BERYL_V3:8]
        exe_file.seekg(-16, std::ios::end);
        uint64_t assets_size = 0;
        exe_file.read(reinterpret_cast<char*>(&assets_size), 8);

        exe_file.seekg(-24, std::ios::end);
        uint64_t payload_size = 0;
        exe_file.read(reinterpret_cast<char*>(&payload_size), 8);

        exe_file.seekg(-28, std::ios::end);
        uint32_t meta_size = 0;
        exe_file.read(reinterpret_cast<char*>(&meta_size), 4);

        std::streamoff meta_offset = -(std::streamoff)(28 + meta_size);
        exe_file.seekg(meta_offset, std::ios::end);
        std::string meta_block(meta_size, '\0');
        exe_file.read(meta_block.data(), meta_size);

        if (meta_size >= 13) {
            meta.is_ui = (uint8_t)meta_block[0] != 0;
            uint32_t w, h, tlen;
            memcpy(&w,    meta_block.data() + 1, 4);
            memcpy(&h,    meta_block.data() + 5, 4);
            memcpy(&tlen, meta_block.data() + 9, 4);
            meta.window_w = w > 0 ? w : 800;
            meta.window_h = h > 0 ? h : 600;
            if (tlen > 0 && 13 + tlen <= meta_size) {
                meta.title = meta_block.substr(13, tlen);
            }
            if (meta_size >= 13 + tlen + 32) {
                meta.has_encryption = true;
                memcpy(meta.encryption_key, meta_block.data() + 13 + tlen, 32);
            }
        }

        // Assets handling
        if (assets_size > 0) {
            std::streamoff assets_offset = -(std::streamoff)(28 + meta_size + payload_size + assets_size);
            exe_file.seekg(assets_offset, std::ios::end);
            std::vector<uint8_t> zip_data(assets_size);
            exe_file.read((char*)zip_data.data(), assets_size);
            
            std::string temp_dir = (fs::temp_directory_path() / generate_random_dir_name()).string();
            if (extract_assets_zip(zip_data, temp_dir)) {
                g_temp_assets_dir = temp_dir;
                fs::current_path(temp_dir);
                std::atexit(cleanup_temp_assets);
            }
        }

        // Payload handling
        std::streamoff payload_offset = -(std::streamoff)(28 + meta_size + payload_size);
        exe_file.seekg(payload_offset, std::ios::end);
        std::vector<char> payload_data(payload_size);
        exe_file.read(payload_data.data(), payload_size);

        if (meta.has_encryption) {
            // Check if key is just zeros (not encrypted)
            bool is_encrypted = false;
            for(int i=0; i<32; i++) if (meta.encryption_key[i] != 0) is_encrypted = true;
            if (is_encrypted) {
                for (size_t i = 0; i < payload_size; i++) {
                    payload_data[i] ^= meta.encryption_key[i % 32];
                }
            }
        }

        std::string payload_str(payload_data.begin(), payload_data.end());
        std::istringstream iss(payload_str);
        *out_function = deserialize_function_from_stream(vm, iss);
        return true;
    }

    if (magic_str == "BERYL_V2") {
        exe_file.seekg(-16, std::ios::end);
        uint64_t payload_size = 0;
        exe_file.read(reinterpret_cast<char*>(&payload_size), 8);

        exe_file.seekg(-20, std::ios::end);
        uint32_t meta_size = 0;
        exe_file.read(reinterpret_cast<char*>(&meta_size), 4);

        std::streamoff meta_offset = -(std::streamoff)(20 + meta_size);
        exe_file.seekg(meta_offset, std::ios::end);
        std::string meta_block(meta_size, '\0');
        exe_file.read(meta_block.data(), meta_size);

        if (meta_size >= 13) {
            meta.is_ui    = (uint8_t)meta_block[0] != 0;
            uint32_t w, h, tlen;
            memcpy(&w,    meta_block.data() + 1, 4);
            memcpy(&h,    meta_block.data() + 5, 4);
            memcpy(&tlen, meta_block.data() + 9, 4);
            meta.window_w = w > 0 ? w : 800;
            meta.window_h = h > 0 ? h : 600;
            if (tlen > 0 && 13 + tlen <= meta_size)
                meta.title = meta_block.substr(13, tlen);
        }

        std::streamoff sbc_offset = -(std::streamoff)(20 + meta_size + payload_size);
        exe_file.seekg(sbc_offset, std::ios::end);
        *out_function = deserialize_function_from_stream(vm, exe_file);
        return true;
    }

    if (magic_str == "BERYL_V1") {
        exe_file.seekg(-16, std::ios::end);
        uint64_t payload_size = 0;
        exe_file.read(reinterpret_cast<char*>(&payload_size), 8);
        exe_file.seekg(-(std::streamoff)(16 + payload_size), std::ios::end);
        *out_function = deserialize_function_from_stream(vm, exe_file);
        meta.is_ui = false;
        return true;
    }

    return false;
}

int main(int argc, char* argv[]) {
    init_terminal();

    std::string exe_path = get_executable_path();
    if (exe_path.empty() && argc > 0) exe_path = argv[0];

    std::ifstream exe_file(exe_path, std::ios::binary | std::ios::ate);

    ObjFunction* main_function = nullptr;
    BerylMeta meta;
    bool found_payload = false;

    if (exe_file.is_open()) {
        std::streamsize file_size = exe_file.tellg();
        VM temp_vm;
        found_payload = parse_beryl_footer(exe_file, file_size, &main_function, &temp_vm, meta);
        exe_file.close();

        if (found_payload && main_function) {
            main_function = nullptr;
        }
    }

    if (!found_payload) {
        if (argc != 2) {
            std::cerr << "Usage: runner <file.sbc>" << std::endl;
            return 1;
        }
        VM vm;
        g_current_vm = &vm;
        ObjFunction* fn = deserialize_function(&vm, argv[1]);
        if (!fn) { std::cerr << "Error: Could not load bytecode." << std::endl; return 1; }
        vm.call_and_run(fn);
        g_current_vm = nullptr;
        return 0;
    }

    // === UI Mode ===
    if (meta.is_ui) {
        sf::RenderWindow window(
            sf::VideoMode({meta.window_w, meta.window_h}),
            meta.title.empty() ? "Sapphire App" : meta.title
        );
        window.setFramerateLimit(60);

        ScriptConfig cfg;
        cfg.windowWidth  = (int)meta.window_w;
        cfg.windowHeight = (int)meta.window_h;
        cfg.windowTitle  = meta.title;

        VM vm(cfg, true, &window);
        g_current_vm = &vm;

        std::ifstream exe2(exe_path, std::ios::binary | std::ios::ate);
        if (!exe2.is_open()) { std::cerr << "Error: Cannot reopen executable." << std::endl; return 1; }
        std::streamsize fs2 = exe2.tellg();
        BerylMeta meta2;
        parse_beryl_footer(exe2, fs2, &main_function, &vm, meta2);
        exe2.close();

        if (!main_function) {
            std::cerr << "Error: Could not load bytecode." << std::endl;
            return 1;
        }

        vm.call_and_run(main_function);
        g_current_vm = nullptr;
        return 0;
    }

    // === Non-UI (console) Mode ===
    {
        VM vm;
        g_current_vm = &vm;

        std::ifstream exe3(exe_path, std::ios::binary | std::ios::ate);
        BerylMeta meta3;
        parse_beryl_footer(exe3, (std::streamsize)exe3.tellg(), &main_function, &vm, meta3);
        exe3.close();

        if (!main_function) {
            std::cerr << "Error: Could not load bytecode." << std::endl;
            return 1;
        }

        vm.call_and_run(main_function);
        g_current_vm = nullptr;
    }

    return 0;
}
