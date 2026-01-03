#include <SFML/Graphics.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <iomanip>
#include <filesystem>
#include <optional>
#include <memory>
#include <atomic>
#include <thread>
#include <fstream>
#include <chrono>


#include "config.h"
#include "vm.h"
#include "compiler.h"
#include "lexer.h"
#include "tokens.h"
#include "httplib.h"
#include "utils.h"
#include "bytecode_io.h"

#include "FileWatch.hpp"

#ifdef _WIN32
#include <windows.h>
void enableVirtualTerminalProcessing() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#endif

int run_compiler(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: sapphire compile <input.sp> <output.sbc>" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];

    std::string source = load_file_as_string(input_path);
    if (source.empty()) {
        std::cerr << "Error: Could not read input file: " << input_path << std::endl;
        return 1;
    }

    bool soft_mode_enabled = check_for_soft_mode(source);

    VM vm;
    vm.soft_mode = soft_mode_enabled;

    ObjFunction* main_function = compile(&vm, source);

    if (main_function == nullptr) {
        std::cerr << "Compilation error in input file." << std::endl;
        return 1;
    }

    serialize_function(main_function, &vm, output_path);
    std::cout << "Compilation completed: " << output_path << std::endl;

    return 0;
}

void display_info() {
    std::vector<std::string> logo = {
        "                         ........                         ",
        "                      ....-:@@......                      ",
        "                  :...-=+=-@%@@.--:.....                  ",
        "              ...:===++++-#@#%@*.====-:.....              ",
        "          .....==+++++++=+@@@@@@-.======--......          ",
        "       ....==+++++++++++#@#:..-%@:-=========-:.....       ",
        "   ....-==++++++++++*#*==.-==+=--*=-:--=========-:....+   ",
        "....==++++++++++*##+=--===++++=+++*##+-.:-==========-:*==.",
        ".-++++++++++++*#+=--===+++++++++++++**#%*=:-===========-..",
        "..+++++++++++++:-==+=+++=++++++++++++++**%@+============#+",
        " .=+++++++++++#-===+=++++++++++++++++++++*@.===========-+ ",
        " .=+++++++++++*==+=++++++++++++++++++++++##.===========.# ",
        " ..+++++++++++++=+++++++++++++++++++++++*%--==========-.. ",
        "  .=+++++++++++#=+=++++++++++++++++++*+*#@.===========:.  ",
        "  ..+++++++++++*+=++++++++++++++++++++*+%+:===========..  ",
        "   .=++++++++++++==++++++++++++++*+*+*+*%.-==========-.   ",
        "   ..++++++++++=++=+++++++++++++++++***+@.:==========..   ",
        "    .=+++++++++-@%-++++++++++++*+*+*+**+@@.-========-.    ",
        "    .=++++++++-+@@:=+++++++++++++**+**+=@@=.========..    ",
        "    ..+++++++==@%@==+++++++*+*+*+*****+*%%@.:======-..    ",
        "     .=+++++=-@%#%%-+++++++++****+****+#%#@@.-=====:.     ",
        "     ..+++++-#@###@-+++++*+*+*+*******+%###@#.-====..     ",
        "      .=+++=+@####@+=++++++**+*+*****+#%###%@-.===-.      ",
        "      ..++=-@%####%%:++++++++++*+*+**+%#####%@.:==..      ",
        "       .==-@%######@=+++++**#####****+%######@@.--.       ",
        "       .--*@######%%@@@@@%%#####%%%@@%%%#####%@+...       ",
        "       ..-@%%%%%%##*++++****##***+++++*##%%%%%%@...       ",
        "        .#@%###***######################***###%@%.        ",
        "         ......-*++++++++++++++++++++++++*-......         ",
        "                ......-+**********+-......                ",
        "                       ............                       "
    };

    std::vector<std::string> info_lines = {
        "",
        ">>> Welcome to Sapphire <<<",
        "",
        "** SAPPHIRE INFORMATIONS: **",
        "",
        "Version: 1.0.7 (build 0076-01032026 (January 3, 2026))",
        "Release Date: January 3, 2026",
        "",
        "Developed by: Bernardo Alvim",
        "Protected by MIT License",
        "",
        "--- Usage Options ---",
        "sapphire <script_path.sp>      : Executes a source script",
        "sapphire <bytecode_path.sbc>   : Executes a bytecode file",
        "sapphire mine get <plugin_name>: Looks for a plugin in the repository 'Mine'",
        "sapphire --info                : Displays this information screen",
        "sapphire compile <input.sp> <output.sbc> : Compiles a script to bytecode",
        "",
        "Thank you!",
        "",
        "GitHub: github.com/foxzyt/sapphire"
    };

    size_t max_logo_width = 0;
    for (const auto& line : logo) {
        if (line.length() > max_logo_width) max_logo_width = line.length();
    }

    const int PADDING = 3;
    for (size_t i = 0; i < std::max(logo.size(), info_lines.size()); ++i) {
        std::cout << std::left;
        if (i < logo.size()) {
            std::cout << logo[i] << std::setw(max_logo_width - logo[i].length() + PADDING) << " ";
        } else {
            std::cout << std::setw(max_logo_width + PADDING) << " ";
        }
        if (i < info_lines.size()) std::cout << info_lines[i];
        std::cout << std::endl;
    }
}

std::string load_source_script(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void load_config_from_script(const std::string& path, ScriptConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        auto delimiterPos = line.find("=");
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            key.erase(std::remove_if(key.begin(), key.end(), isspace), key.end());
            value.erase(std::remove_if(value.begin(), value.end(), isspace), value.end());
            if (key.find("config_window_width") != std::string::npos) {
                try { config.windowWidth = std::stoi(value); } catch(...) {}
            } else if (key.find("config_window_height") != std::string::npos) {
                try { config.windowHeight = std::stoi(value); } catch(...) {}
            } else if (key.find("config_window_title") != std::string::npos) {
                 auto start_quote = line.find("\"");
                 auto end_quote = line.rfind("\"");
                 if (start_quote != std::string::npos && end_quote != start_quote) {
                     config.windowTitle = line.substr(start_quote + 1, end_quote - start_quote - 1);
                 }
            }
        }
    }
}

bool script_uses_ui(const std::string& source) {
    Lexer lexer(source);
    for (;;) {
        Token token = lexer.scan_token();
        if (token.type == TokenType::TOKEN_IDENTIFIER && token.literal == "UI") return true;
        if (token.type == TokenType::TOKEN_END_OF_FILE) break;
    }
    return false;
}

std::filesystem::file_time_type get_last_write(const std::string& path) {
    return std::filesystem::last_write_time(path);
}

void run_ui_mode(const std::string& script_path, const ScriptConfig& config) {
    sf::RenderWindow window(sf::VideoMode({(unsigned int)config.windowWidth, (unsigned int)config.windowHeight}), config.windowTitle);
    window.setFramerateLimit(60);

    std::filesystem::path absolute_path = std::filesystem::absolute(script_path);
    std::string parent_dir = absolute_path.parent_path().string();
    std::string target_file = absolute_path.filename().string();

    std::unique_ptr<VM> vm;
    std::atomic<bool> should_reload{false};

    // Variavel para controlar o tempo do ultimo reload
    auto last_reload_time = std::chrono::steady_clock::now();

    try {
        filewatch::FileWatch<std::string> watch(
            parent_dir,
            [&](const std::string& path, const filewatch::Event change_type) {
                if (path == target_file) should_reload = true;
            }
        );

        auto perform_reload = [&]() {
            // Limpa o console para facilitar a leitura do novo reload
            #ifdef _WIN32
            system("cls");
            #else
            system("clear");
            #endif
            std::cout << "[Sapphire] Reloading script..." << std::endl;

            std::string content = load_source_script(absolute_path.string());
            if (content.empty()) return;

            auto new_vm = std::make_unique<VM>(config, true, &window);
            new_vm->soft_mode = check_for_soft_mode(content);
            g_current_vm = new_vm.get();

            ObjFunction* new_func = compile(new_vm.get(), content);

            if (new_func) {
                // Executa o escopo global (definições)
                if (new_vm->call_and_run(new_func)) {
                    vm = std::move(new_vm);
                    std::cout << "[Sapphire] Reload success." << std::endl;
                } else {
                    if (vm) g_current_vm = vm.get();
                    std::cerr << "[Sapphire] Runtime error during reload." << std::endl;
                }
            } else {
                if (vm) g_current_vm = vm.get();
                std::cerr << "[Sapphire] Compilation error." << std::endl;
            }
        };

        perform_reload();

        while (window.isOpen()) {
            // FIX: Só processa eventos no main.cpp SE a VM não existir (erro de compilação).
            // Se a VM existe, quem deve processar eventos é o UI.Begin() dentro do script.
            if (!vm) {
                while (const std::optional event = window.pollEvent()) {
                    if (event->is<sf::Event::Closed>()) window.close();
                }

                // Tela de erro (vermelha) se não houver VM válida
                window.clear(sf::Color::Red);
                window.display();
                continue;
            }

            // Lógica de Hot-Reload
            if (should_reload) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_reload_time).count();

                if (elapsed > 500) {
                    perform_reload();
                    last_reload_time = now;
                    should_reload = false;
                }
            }

            // FIX: Removemos window.clear() daqui. O native_ui_begin (vm.cpp) já faz isso.

            vm->resetStack();
            vm->ui_state.nextPosX = 10.0f;
            vm->ui_state.nextPosY = 10.0f;

            SapphireValue update_fn_val = vm->getGlobal("updateUI");
            if (std::holds_alternative<Obj*>(update_fn_val._value)) {
                Obj* obj = std::get<Obj*>(update_fn_val._value);
                if (obj && obj->type == OBJ_CLOSURE) {
                    vm->call_and_run(static_cast<ObjClosure*>(obj)->function);
                }
            }

            // FIX: Removemos window.display() daqui. O native_ui_end (vm.cpp) já faz isso.
            // Se você chamar display() aqui de novo, causa o flickering (double buffer swap).
        }

    } catch (...) {
        std::cerr << "[Sapphire] Fatal watcher error." << std::endl;
    }
}

void run_file_mode(const std::string& script_content) {
    VM vm;
    g_current_vm = &vm;
    vm.soft_mode = check_for_soft_mode(script_content);
    (void)vm.interpret(script_content);
    g_current_vm = nullptr;
}

void run_bytecode_mode(const std::string& bytecode_path, const ScriptConfig& config, bool is_ui_mode) {
    sf::RenderWindow* window_ptr = nullptr;
    std::optional<sf::RenderWindow> temp_window;

    if (is_ui_mode) {
        temp_window.emplace(sf::VideoMode({config.windowWidth, config.windowHeight}), config.windowTitle);
        temp_window->setFramerateLimit(60);
        window_ptr = &(*temp_window);
    }

    VM vm(config, is_ui_mode, window_ptr);
    g_current_vm = &vm;

    ObjFunction* main_bytecode_function = deserialize_function(&vm, bytecode_path);
    if (main_bytecode_function == nullptr) {
        if (temp_window) temp_window->close();
        g_current_vm = nullptr;
        return;
    }

    vm.run_function(main_bytecode_function);

    if (is_ui_mode && temp_window) {
        SapphireValue update_fn_val = vm.getGlobal("updateUI");
        if (std::holds_alternative<Obj*>(update_fn_val._value) && std::get<Obj*>(update_fn_val._value)->type == OBJ_CLOSURE) {
            ObjFunction* update_function = static_cast<ObjClosure*>(std::get<Obj*>(update_fn_val._value))->function;

            while (temp_window->isOpen()) {
                while (const std::optional event = temp_window->pollEvent()) {
                    if (event->is<sf::Event::Closed>()) temp_window->close();
                }
                if (!temp_window->isOpen()) break;

                vm.ui_state.nextPosX = 10.0f;
                vm.ui_state.nextPosY = 10.0f;

                temp_window->clear(sf::Color(25, 25, 30));
                if (!vm.call_and_run(update_function)) {
                    temp_window->close();
                    break;
                }
                temp_window->display();
            }
        }
    }
    g_current_vm = nullptr;
}

void download_plugin(const std::string& plugin_name) {
    const std::string host = "https://raw.githubusercontent.com";
    const std::string path = "/foxzyt/sapphire-mine/main/" + plugin_name + ".sp";

    httplib::Client cli(host);
    cli.set_follow_location(true);
    auto res = cli.Get(path.c_str());

    if (!res || res->status != 200) return;

    const char* appdata = getenv("APPDATA");
    if (!appdata) return;

    std::filesystem::path plugin_dir = std::filesystem::path(appdata) / "Sapphire" / "plugins";
    std::filesystem::create_directories(plugin_dir);
    std::ofstream(plugin_dir / (plugin_name + ".sp")) << res->body;
}

int main(int argc, char* argv[]) {
    if (argc == 4 && std::string(argv[1]) == "mine" && std::string(argv[2]) == "get") {
        download_plugin(argv[3]);
    } else if (argc == 4 && std::string(argv[1]) == "compile") {
        return run_compiler(argc - 1, &argv[1]);
    } else if (argc == 2 && std::string(argv[1]) == "--info") {
        display_info();
    } else if (argc == 2) {
        std::string path = argv[1];

        std::filesystem::path fs_path(path);
        if (!std::filesystem::exists(fs_path)) {
            return 1;
        }

        std::string ext = fs_path.extension().string();
        ScriptConfig config;
        load_config_from_script(path, config);

        if (ext == ".sbc") {
            bool is_ui = (path.find("ui") != std::string::npos);
            run_bytecode_mode(path, config, is_ui);
        } else if (ext == ".sp") {
            std::string content = load_source_script(path);

            if (content.empty()) {
                return 1;
            }

            bool has_ui = script_uses_ui(content);

            if (has_ui) {
                run_ui_mode(path, config);
            }
            else {
                run_file_mode(content);
            }
        }
    } else {
        std::cerr << "Usage: sapphire <script.sp/sbc> | compile <in> <out> | --info | mine get <name>\n";
        return 64;
    }

    return 0;
}