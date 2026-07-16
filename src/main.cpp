#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

#include "bytecode_io.h"
#include "compiler.h"
#include "compiler/debug.h"
#include "config.h"
#include "lexer.h"
#include "preprocessor/preprocessor.h"
#include "termcolor.h"
#include "tokens.h"
#include "utils.h"
#include "vm.h"

// Include httplib last to avoid Windows.h namespace pollution
#include "httplib.h"

int run_compiler(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: sapphire compile <input.sp> <output.sbc>" << std::endl;
    return 1;
  }

  std::string input_path = argv[1];
  std::string output_path = argv[2];

  std::string source = load_file_as_string(input_path);
  if (source.empty()) {
    std::cerr << "Error: Could not read input file: " << input_path
              << std::endl;
    return 1;
  }

  bool soft_mode_enabled = check_for_soft_mode(source);

  VM vm;
  vm.soft_mode = soft_mode_enabled;

  Preprocessor prep;
  std::string processed_source = prep.process(source);
  ObjFunction *main_function = compile(&vm, processed_source);

  if (main_function == nullptr) {
    std::cerr << "Compilation error in input file." << std::endl;
    return 1;
  }

  serialize_function(main_function, &vm, output_path);
  std::cout << "Compilation completed: " << output_path << std::endl;

  return 0;
}

void display_info() {
  std::vector<std::string> info_lines = {
      "",
      ">>> Welcome to Sapphire <<<",
      "",
      "** SAPPHIRE INFORMATIONS: **",
      "",
      "Version: 1.0.9 (build 0715-07152026 (July 15, 2026))",
      "Release Date: July 15, 2026",
      "",
      "Developed by: Bernardo Alvim",
      "Protected by MIT License",
      "",
      "--- Usage Options ---",
      "sapphire                         : Starts the interactive REPL",
      "sapphire <script_path.sp>        : Executes a source script",
      "sapphire run <script_path.sp>    : Executes a source script",
      "sapphire <bytecode_path.sbc>     : Executes a bytecode file",
      "sapphire -e \"<code>\"             : Executes inline code",
      "sapphire check <script_path.sp>  : Checks syntax without running",
      "sapphire disasm <script_path.sp> : Prints VM bytecode disassembly",
      "sapphire init <project_name>     : Initializes a new project skeleton",
      "sapphire clean                   : Removes all .sbc generated files",
      "sapphire compile <in> <out>      : Compiles a script to bytecode",
      "sapphire -v | --version          : Displays version information",
      "sapphire -h | --help | --info    : Displays this information screen",
      "",
      "Thank you!",
      "",
      "GitHub: github.com/foxzyt/sapphire"};

  for (const auto &line : info_lines) {
    std::cout << line << std::endl;
  }
}

void display_version() {
  std::cout << "Sapphire 1.0.9 (build 0715-07152026)" << std::endl;
}

std::string load_source_script(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return "";
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void load_config_from_script(const std::string &path, ScriptConfig &config) {
  std::ifstream file(path);
  if (!file.is_open())
    return;
  std::string line;
  while (std::getline(file, line)) {
    size_t comment_pos = line.find("//");
    if (comment_pos != std::string::npos)
      line = line.substr(0, comment_pos);
    auto delimiterPos = line.find("=");
    if (delimiterPos != std::string::npos) {
      std::string key = line.substr(0, delimiterPos);
      std::string value = line.substr(delimiterPos + 1);
      key.erase(std::remove_if(key.begin(), key.end(), isspace), key.end());
      value.erase(std::remove_if(value.begin(), value.end(), isspace),
                  value.end());
      if (key.find("config_window_width") != std::string::npos) {
        try {
          config.windowWidth = std::stoi(value);
        } catch (...) {
        }
      } else if (key.find("config_window_height") != std::string::npos) {
        try {
          config.windowHeight = std::stoi(value);
        } catch (...) {
        }
      } else if (key.find("config_window_borderless") != std::string::npos) {
        if (value == "true" || value == "1")
          config.windowBorderless = true;
        else
          config.windowBorderless = false;
      } else if (key.find("config_window_title") != std::string::npos) {
        auto start_quote = line.find("\"");
        auto end_quote = line.rfind("\"");
        if (start_quote != std::string::npos && end_quote != start_quote) {
          config.windowTitle =
              line.substr(start_quote + 1, end_quote - start_quote - 1);
        }
      }
    }
  }
}

bool script_uses_ui(const std::string &source) {
  Lexer lexer(source);
  for (;;) {
    Token token = lexer.scan_token();
    if (token.type == TokenType::TOKEN_IDENTIFIER && token.literal == "UI")
      return true;
    if (token.type == TokenType::TOKEN_END_OF_FILE)
      break;
  }
  return false;
}

void run_ui_mode(std::string script_content, const ScriptConfig &config,
                 const std::string &script_path) {
  uint32_t style =
      config.windowBorderless ? sf::Style::None : sf::Style::Default;
  sf::RenderWindow window(
      sf::VideoMode({config.windowWidth, config.windowHeight}),
      config.windowTitle, style);
  window.setFramerateLimit(60);

  bool soft_mode_enabled = check_for_soft_mode(script_content);
  VM vm(config, true, &window);
  g_current_vm = &vm;
  vm.soft_mode = soft_mode_enabled;
  vm.add_module_search_path(
      std::filesystem::path(script_path).parent_path().string());

  Preprocessor prep;
  script_content = prep.process(script_content);
  ObjFunction *main_script_func = compile(&vm, script_content);
  if (main_script_func == nullptr) {
    window.close();
    return;
  }

  vm.call_and_run(main_script_func);

  SapphireValue update_fn_val = vm.getGlobal("updateUI");
  if (!std::holds_alternative<Obj *>(update_fn_val._value) ||
      std::get<Obj *>(update_fn_val._value)->type != OBJ_CLOSURE) {
    std::cerr << "Error: updateUI() function not found in script." << std::endl;
    window.close();
    return;
  }

  ObjClosure *update_closure =
      static_cast<ObjClosure *>(std::get<Obj *>(update_fn_val._value));
  ObjFunction *update_function = update_closure->function;

  auto last_write = std::filesystem::last_write_time(script_path);

  while (window.isOpen()) {
    try {
      auto current_write = std::filesystem::last_write_time(script_path);
      if (current_write > last_write) {
        last_write = current_write;
        std::cout << "Reloading script: " << script_path << std::endl;
        std::string new_content = load_source_script(script_path);
        new_content = prep.process(new_content);
        ObjFunction *new_func = compile(&vm, new_content);
        if (new_func) {
          vm.call_and_run(new_func);
          SapphireValue new_update_fn_val = vm.getGlobal("updateUI");
          if (std::holds_alternative<Obj *>(new_update_fn_val._value) &&
              std::get<Obj *>(new_update_fn_val._value)->type == OBJ_CLOSURE) {
            update_closure = static_cast<ObjClosure *>(
                std::get<Obj *>(new_update_fn_val._value));
            update_function = update_closure->function;
          }
        }
      }
    } catch (const std::filesystem::filesystem_error &) {
    }

    vm.resetStack();

    if (!vm.call_and_run(update_function)) {
      break;
    }
  }
  g_current_vm = nullptr;
}

void run_file_mode(const std::string &script_content,
                   const std::string &script_path) {
  VM vm;
  g_current_vm = &vm;
  vm.soft_mode = check_for_soft_mode(script_content);
  vm.add_module_search_path(
      std::filesystem::path(script_path).parent_path().string());
  Preprocessor prep;
  std::string processed = prep.process(script_content);
  (void)vm.interpret(processed);
  g_current_vm = nullptr;
}

void run_repl() {
  std::cout << "Sapphire REPL v1.0.9\nType 'exit' or 'quit' to close.\n";
  VM vm;
  g_current_vm = &vm;
  std::string line;
  while (true) {
    std::cout << ">> ";
    if (!std::getline(std::cin, line))
      break;
    if (line == "exit" || line == "quit")
      break;
    if (line.empty())
      continue;

    ObjFunction *func = compile(&vm, line);
    if (func) {
      vm.call_and_run(func);
      vm.resetStack();
    }
  }
  g_current_vm = nullptr;
}

void run_eval(const std::string &code) {
  VM vm;
  g_current_vm = &vm;
  ObjFunction *func = compile(&vm, code);
  if (func) {
    vm.call_and_run(func);
  }
  g_current_vm = nullptr;
}

void run_check(const std::string &path) {
  std::string source = load_source_script(path);
  if (source.empty()) {
    std::cerr << "Error: Could not read " << path << std::endl;
    return;
  }
  VM vm;
  Preprocessor prep;
  std::string processed = prep.process(source);
  ObjFunction *func = compile(&vm, processed);
  if (func) {
    std::cout << "Syntax OK: " << path << std::endl;
  } else {
    std::cerr << "Syntax Error: " << path << std::endl;
  }
}

void run_disasm(const std::string &path) {
  std::string source = load_source_script(path);
  if (source.empty()) {
    std::cerr << "Error: Could not read " << path << std::endl;
    return;
  }
  VM vm;
  Preprocessor prep;
  std::string processed = prep.process(source);
  ObjFunction *func = compile(&vm, processed);
  if (func) {
    disassemble_chunk(func->chunk, path);
  } else {
    std::cerr << "Compilation failed." << std::endl;
  }
}

void run_clean() {
  int count = 0;
  try {
    for (const auto &entry : std::filesystem::directory_iterator(".")) {
      if (entry.path().extension() == ".sbc") {
        std::filesystem::remove(entry.path());
        count++;
      }
    }
    std::cout << "Cleaned " << count << " bytecode files." << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error during clean: " << e.what() << std::endl;
  }
}

void run_init(const std::string &name) {
  if (std::filesystem::exists(name)) {
    std::cerr << "Error: Directory '" << name << "' already exists."
              << std::endl;
    return;
  }
  try {
    std::filesystem::create_directory(name);
    std::filesystem::create_directory(name + "/build");

    std::ofstream info(name + "/ProjectInfo.txt");
    info << "Project=" << name << "\n"
         << "Author=Sapphire Developer\n"
         << "Version=1.0.0\n"
         << "Build=1\n"
         << "OutputFile=build/app.exe\n"
         << "Date=" << std::time(nullptr) << "\n";
    info.close();

    std::ofstream theme(name + "/theme.sp");
    theme << "// Theme definitions\n"
          << "var window_width = 800;\n"
          << "var window_height = 600;\n"
          << "var window_title = \"" << name << "\";\n"
          << "var primary_color = \"blue\";\n";
    theme.close();

    std::ofstream main(name + "/main.sp");
    main << "import \"theme.sp\";\n\n"
         << "var config_window_width = window_width;\n"
         << "var config_window_height = window_height;\n"
         << "var config_window_title = window_title;\n"
         << "var UI = 1;\n\n"
         << "function updateUI() {\n"
         << "    pollEvents();\n"
         << "    if (isWindowOpen() == false) { return false; }\n"
         << "    clear();\n"
         << "    display();\n"
         << "    return true;\n"
         << "}\n";
    main.close();

    std::cout << "Initialized Sapphire project in '" << name << "'."
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error initializing project: " << e.what() << std::endl;
  }
}

void run_bytecode_mode(const std::string &bytecode_path,
                       const ScriptConfig &config, bool is_ui_mode) {
  sf::RenderWindow *window_ptr = nullptr;
  std::optional<sf::RenderWindow> temp_window;

  if (is_ui_mode) {
    uint32_t style =
        config.windowBorderless ? sf::Style::None : sf::Style::Default;
    temp_window.emplace(
        sf::VideoMode({config.windowWidth, config.windowHeight}),
        config.windowTitle, style);
    temp_window->setFramerateLimit(60);
    window_ptr = &(*temp_window);
  }

  VM vm(config, is_ui_mode, window_ptr);
  vm.add_module_search_path(
      std::filesystem::path(bytecode_path).parent_path().string());
  g_current_vm = &vm;

  ObjFunction *main_bytecode_function =
      deserialize_function(&vm, bytecode_path);
  if (main_bytecode_function == nullptr) {
    if (temp_window)
      temp_window->close();
    g_current_vm = nullptr;
    return;
  }

  vm.run_function(main_bytecode_function);

  if (is_ui_mode && temp_window) {
    SapphireValue update_fn_val = vm.getGlobal("updateUI");
    if (std::holds_alternative<Obj *>(update_fn_val._value) &&
        std::get<Obj *>(update_fn_val._value)->type == OBJ_CLOSURE) {
      ObjFunction *update_function =
          static_cast<ObjClosure *>(std::get<Obj *>(update_fn_val._value))
              ->function;

      while (temp_window->isOpen()) {
        while (const std::optional event = temp_window->pollEvent()) {
          if (event->is<sf::Event::Closed>())
            temp_window->close();
        }
        if (!temp_window->isOpen())
          break;

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

int main(int argc, char *argv[]) {
  init_terminal();

  if (argc == 1) {
    run_repl();
    return 0;
  }

  std::string command = argv[1];

  if (command == "-h" || command == "--help" || command == "--info") {
    display_info();
  } else if (command == "-v" || command == "--version") {
    display_version();
  } else if (command == "compile" && argc == 4) {
    return run_compiler(argc - 1, &argv[1]);
  } else if (command == "eval" || command == "-e") {
    if (argc >= 3)
      run_eval(argv[2]);
    else
      std::cerr << "Usage: sapphire eval \"<code>\"" << std::endl;
  } else if (command == "check") {
    if (argc >= 3)
      run_check(argv[2]);
    else
      std::cerr << "Usage: sapphire check <file>" << std::endl;
  } else if (command == "disasm") {
    if (argc >= 3)
      run_disasm(argv[2]);
    else
      std::cerr << "Usage: sapphire disasm <file>" << std::endl;
  } else if (command == "clean") {
    run_clean();
  } else if (command == "init") {
    if (argc >= 3)
      run_init(argv[2]);
    else
      std::cerr << "Usage: sapphire init <project_name>" << std::endl;
  } else {
    std::string path = (command == "run" && argc >= 3) ? argv[2] : command;
    std::string ext = std::filesystem::path(path).extension().string();
    ScriptConfig config;
    load_config_from_script(path, config);

    if (ext == ".sbc") {
      bool is_ui = (path.find("ui") != std::string::npos);
      run_bytecode_mode(path, config, is_ui);
    } else if (ext == ".sp") {
      std::string content = load_source_script(path);
      if (content.empty())
        return 1;
      if (script_uses_ui(content))
        run_ui_mode(content, config, path);
      else
        run_file_mode(content, path);
    } else {
      std::cerr << "Error: Unknown command or invalid file extension: "
                << command << std::endl;
      return 1;
    }
  }

  return 0;
}