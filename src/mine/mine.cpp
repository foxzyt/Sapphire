#include "httplib.h"
#include "termcolor.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// Helpers
fs::path get_plugin_dir() {
  const char *appdata = getenv("APPDATA");
  if (!appdata)
    return "";
  fs::path plugin_dir = fs::path(appdata) / "Sapphire" / "plugins";
  fs::create_directories(plugin_dir);
  return plugin_dir;
}

void print_info() {
  std::cout << tc_bold() << tc_blue() << "Mine Package Manager" << tc_reset()
            << " for Sapphire\n";
  std::cout << "Version: 1.0.0\n";
  std::cout << "Repository: https://github.com/foxzyt/sapphire-mine\n\n";

  std::cout << tc_bold() << "Commands:\n" << tc_reset();
  std::cout << "  " << tc_green() << "mine get <name>" << tc_reset()
            << "    - Download and install a plugin\n";
  std::cout << "  " << tc_yellow() << "mine remove <name>" << tc_reset()
            << " - Remove an installed plugin\n";
  std::cout << "  " << tc_cyan() << "mine list" << tc_reset()
            << "          - List installed plugins\n";
  std::cout << "  " << tc_blue() << "mine info" << tc_reset()
            << "          - Show this information\n";
}

void list_plugins() {
  fs::path dir = get_plugin_dir();
  if (dir.empty()) {
    std::cerr << tc_red() << "Error: Could not access APPDATA.\n" << tc_reset();
    return;
  }

  std::cout << tc_bold() << tc_cyan() << "Installed Plugins:\n" << tc_reset();
  bool found = false;
  for (const auto &entry : fs::directory_iterator(dir)) {
    if (entry.path().extension() == ".sp") {
      std::cout << "  - " << tc_green() << entry.path().stem().string()
                << tc_reset() << "\n";
      found = true;
    }
  }
  if (!found) {
    std::cout << "  " << tc_yellow() << "(No plugins installed yet)"
              << tc_reset() << "\n";
  }
}

void remove_plugin(const std::string &name) {
  fs::path dir = get_plugin_dir();
  fs::path file = dir / (name + ".sp");

  if (fs::exists(file)) {
    fs::remove(file);
    std::cout << tc_green() << "Success:" << tc_reset() << " Removed plugin '"
              << name << "'.\n";
  } else {
    std::cout << tc_yellow() << "Warning:" << tc_reset() << " Plugin '" << name
              << "' is not installed.\n";
  }
}

void get_plugin(const std::string &name) {
  const std::string host = "https://raw.githubusercontent.com";
  const std::string path = "/foxzyt/sapphire-mine/main/" + name + ".sp";

  std::cout << tc_cyan() << "Connecting to repository..." << tc_reset() << "\n";

  httplib::Client cli(host);
  cli.set_follow_location(true);

  std::cout << tc_blue() << "Downloading '" << name << "'..." << tc_reset()
            << "\n";
  auto res = cli.Get(path.c_str());

  if (!res) {
    std::cerr << tc_red() << "Error:" << tc_reset() << " Connection failed.\n";
    return;
  }
  if (res->status != 200) {
    std::cerr << tc_red() << "Error:" << tc_reset() << " Plugin '" << name
              << "' not found in repository (HTTP " << res->status << ").\n";
    return;
  }

  fs::path dir = get_plugin_dir();
  std::ofstream out_file(dir / (name + ".sp"));
  if (out_file.is_open()) {
    out_file << res->body;
    std::cout << tc_bold() << tc_green() << "Successfully installed '" << name
              << "'!" << tc_reset() << "\n";
  } else {
    std::cerr << tc_red() << "Error:" << tc_reset()
              << " Failed to write file to APPDATA.\n";
  }
}

int main(int argc, char *argv[]) {
  init_terminal();

  if (argc < 2) {
    print_info();
    return 0;
  }

  std::string cmd = argv[1];

  if (cmd == "info") {
    print_info();
  } else if (cmd == "list") {
    list_plugins();
  } else if (cmd == "get") {
    if (argc < 3) {
      std::cerr << tc_red() << "Error:" << tc_reset()
                << " Missing plugin name.\n";
      std::cerr << "Usage: mine get <name>\n";
      return 1;
    }
    get_plugin(argv[2]);
  } else if (cmd == "remove") {
    if (argc < 3) {
      std::cerr << tc_red() << "Error:" << tc_reset()
                << " Missing plugin name.\n";
      std::cerr << "Usage: mine remove <name>\n";
      return 1;
    }
    remove_plugin(argv[2]);
  } else {
    std::cerr << tc_red() << "Error:" << tc_reset() << " Unknown command '"
              << cmd << "'.\n";
    print_info();
    return 1;
  }

  return 0;
}
