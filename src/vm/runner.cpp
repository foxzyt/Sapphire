#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include "vm.h"
#include "object.h"
#include "utils.h"
#include "termcolor.h"
#include "value.h"
#include "bytecode_io.h"
#include "tokens.h"
#include "config.h"
#include <SFML/Graphics.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

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
};

// Parse BERYL_V2 metadata from the open exe file.
// Returns true and fills meta if BERYL_V2. Returns false if BERYL_V1 (no meta).
// On return, exe_file is positioned at start of sbc payload.
bool parse_beryl_footer(std::ifstream& exe_file, std::streamsize file_size,
                        ObjFunction** out_function, VM* vm, BerylMeta& meta) {
    if (file_size < 20) return false;

    // Read last 8 bytes: magic
    exe_file.seekg(-8, std::ios::end);
    char magic[9] = {0};
    exe_file.read(magic, 8);
    std::string magic_str(magic, 8);

    if (magic_str == "BERYL_V2") {
        // Footer: [meta][meta_size:4][payload_size:8][BERYL_V2:8]
        exe_file.seekg(-16, std::ios::end);
        uint64_t payload_size = 0;
        exe_file.read(reinterpret_cast<char*>(&payload_size), 8);

        exe_file.seekg(-20, std::ios::end);
        uint32_t meta_size = 0;
        exe_file.read(reinterpret_cast<char*>(&meta_size), 4);

        // Read metadata block
        std::streamoff meta_offset = -(std::streamoff)(20 + meta_size);
        exe_file.seekg(meta_offset, std::ios::end);
        std::string meta_block(meta_size, '\0');
        exe_file.read(meta_block.data(), meta_size);

        // Parse meta: [is_ui:1][w:4][h:4][tlen:4][title:N]
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

        // Seek to sbc payload start
        std::streamoff sbc_offset = -(std::streamoff)(20 + meta_size + payload_size);
        exe_file.seekg(sbc_offset, std::ios::end);
        *out_function = deserialize_function_from_stream(vm, exe_file);
        return true;
    }

    if (magic_str == "BERYL_V1") {
        // Legacy: no UI meta, non-UI app
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
        ScriptConfig dummy_cfg;
        VM temp_vm;
        found_payload = parse_beryl_footer(exe_file, file_size, &main_function, &temp_vm, meta);
        exe_file.close();

        if (found_payload && main_function) {
            // Re-deserialize with a proper VM (temp_vm was just for detection)
            // Actually we need to use the VM that runs the script — let's set up properly.
            // We'll redo deserialization with the correct VM below.
            main_function = nullptr; // will redo with correct VM
        }
    }

    if (!found_payload) {
        // Fallback: run as plain bytecode runner
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

        // Re-open and deserialize with the correct VM
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

        // Run the script — the script body contains while(true) { updateUI(); }
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
