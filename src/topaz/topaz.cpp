#include "httplib.h"
#include "termcolor.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Helpers
fs::path get_plugin_dir() {
  const char *config_dir = nullptr;
#ifdef _WIN32
  config_dir = getenv("APPDATA");
#elif defined(__APPLE__)
  config_dir = getenv("HOME");
  if (config_dir) {
    static std::string path = std::string(config_dir) + "/Library/Application Support";
    config_dir = path.c_str();
  }
#elif defined(__linux__)
  config_dir = getenv("XDG_DATA_HOME");
  if (!config_dir) {
    config_dir = getenv("HOME");
    if (config_dir) {
      static std::string path = std::string(config_dir) + "/.local/share";
      config_dir = path.c_str();
    }
  }
#endif

  if (!config_dir)
    return "";

  fs::path plugin_dir = fs::path(config_dir) / "Sapphire" / "plugins";
  fs::create_directories(plugin_dir);
  return plugin_dir;
}

void print_info() {
  std::cout << tc_bold() << tc_blue() << "Topaz Package Manager" << tc_reset()
            << " for Sapphire\n";
  std::cout << "Version: 1.0.0\n";
  std::cout << "Repository: https://github.com/foxzyt/sapphire-topaz\n\n";

  std::cout << tc_bold() << "Commands:\n" << tc_reset();
  std::cout << "  " << tc_green() << "topaz get <name>" << tc_reset()
            << "    - Download and install a plugin from the repository\n";
  std::cout << "  " << tc_green() << "topaz install <name>" << tc_reset()
            << " - Install local file OR download from repository\n";
  std::cout << "  " << tc_yellow() << "topaz remove <name>" << tc_reset()
            << "  - Remove an installed plugin\n";
  std::cout << "  " << tc_cyan() << "topaz list" << tc_reset()
            << "           - List installed plugins\n";
  std::cout << "  " << tc_blue() << "topaz info" << tc_reset()
            << "           - Show this information\n";
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

void get_plugin(const std::string &raw_name) {
  std::string name = raw_name;
  if (name.length() >= 3 && name.substr(name.length() - 3) == ".sp") {
    name = name.substr(0, name.length() - 3);
  }

  const std::string host = "https://raw.githubusercontent.com";
  const std::string path = "/foxzyt/sapphire-topaz/main/" + name + ".sp";

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

void install_local_plugin(const std::string &filepath) {
  fs::path file(filepath);
  if (!fs::exists(file)) {
    // Fallback to downloading from GitHub
    get_plugin(filepath);
    return;
  }
  if (file.extension() != ".sp") {
    std::cerr << tc_red() << "Error:" << tc_reset() << " Plugin must be a .sp file.\n";
    return;
  }

  fs::path dir = get_plugin_dir();
  fs::path target = dir / file.filename();

  try {
    fs::copy_file(file, target, fs::copy_options::overwrite_existing);
    std::cout << tc_bold() << tc_green() << "Successfully installed local plugin '" 
              << file.stem().string() << "'!" << tc_reset() << "\n";
  } catch (const std::exception &e) {
    std::cerr << tc_red() << "Error:" << tc_reset() << " Failed to install plugin: " << e.what() << "\n";
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
      std::cerr << "Usage: topaz get <name>\n";
      return 1;
    }
    get_plugin(argv[2]);
  } else if (cmd == "install") {
    if (argc < 3) {
      std::cerr << tc_red() << "Error:" << tc_reset()
                << " Missing plugin name or file path.\n";
      std::cerr << "Usage: topaz install <name>\n";
      return 1;
    }
    install_local_plugin(argv[2]);
  } else if (cmd == "remove") {
    if (argc < 3) {
      std::cerr << tc_red() << "Error:" << tc_reset()
                << " Missing plugin name.\n";
      std::cerr << "Usage: topaz remove <name>\n";
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
