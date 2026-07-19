#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>


// Core headers
#include "core/downloader.hpp"
#include "core/fs_utils.hpp"
#include "core/parser.hpp"
#include "core/resolver.hpp"
#include "core/sapphire_version.hpp"
#include "core/semver.hpp"
#include "core/types.hpp"


// Command headers
#include "commands/cache_clean.hpp"
#include "commands/cache_dir.hpp"
#include "commands/check.hpp"
#include "commands/expand.hpp"
#include "commands/info.hpp"
#include "commands/init.hpp"
#include "commands/install.hpp"
#include "commands/list.hpp"
#include "commands/lock.hpp"
#include "commands/outdated.hpp"
#include "commands/purge.hpp"
#include "commands/sapphire_cmd.hpp"
#include "commands/search.hpp"
#include "commands/tree.hpp"
#include "commands/uninstall.hpp"
#include "commands/update.hpp"
#include "commands/upgrade.hpp"
#include "commands/test.hpp"
#include "commands/clean.hpp"


#include "termcolor.hpp"

namespace topaz {

void print_help() {
  std::cout << termcolor::cyan << termcolor::bold
            << "Topaz Package Manager for Sapphire" << termcolor::reset
            << std::endl;
  std::cout << "Version: 2.4.0" << std::endl;
  std::cout << "Repository: https://github.com/foxzyt/sapphire-topaz"
            << std::endl;
  std::cout << std::endl;

  std::cout << termcolor::bold << "Topaz Self-Update:" << termcolor::reset
            << std::endl;
  std::cout
      << "  " << termcolor::yellow << "topaz upgrade" << termcolor::reset
      << "                 - Download and install the latest topaz version"
      << std::endl;
  std::cout << std::endl;

  std::cout << termcolor::bold << "Project Workflow Commands:" << termcolor::reset << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz test [dir]" << termcolor::reset
            << "              - Run tests (default: tests/ directory)" << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz clean" << termcolor::reset
            << "                   - Clean build artifacts" << std::endl;
  std::cout << std::endl;

  std::cout << termcolor::bold << "Plugin/Project Commands:" << termcolor::reset
            << std::endl;
  std::cout << "  " << termcolor::green << "topaz init [options]"
            << termcolor::reset
            << "         - Create a new project structure (sapphire.json)"
            << std::endl;
  std::cout << "    Options:" << std::endl;
  std::cout << "      -y, --yes               - Skip interactive prompts"
            << std::endl;
  std::cout << "      --name <name>           - Pre-specify project/plugin name"
            << std::endl;
  std::cout << "      --version <ver>         - Pre-specify initial version"
            << std::endl;
  std::cout << "      --author <author>       - Pre-specify author"
            << std::endl;
  std::cout << "      --description <desc>    - Pre-specify description"
            << std::endl;
  std::cout << std::endl;
  std::cout << "  " << termcolor::green << "topaz expand <version>"
            << termcolor::reset << "         - Add a new version to the plugin"
            << std::endl;
  std::cout << std::endl;
  std::cout << "  " << termcolor::green
            << "topaz install [name] [version] [options]" << std::endl;
  std::cout << "  " << termcolor::green << "topaz install" << termcolor::reset
            << "                 - Install all dependencies from "
               "sapphire.json/topaz.lock"
            << std::endl;
  std::cout << "    Options:" << std::endl;
  std::cout << "      --local                 - Force local installation scope "
               "(./plugins/)"
            << std::endl;
  std::cout << "      --global                - Force global installation "
               "scope (AppData)"
            << std::endl;
  std::cout
      << "      -D, --save-dev          - Save as a development dependency"
      << std::endl;
  std::cout
      << "      --no-save               - Skip writing manifest and lockfiles"
      << std::endl;
  std::cout << "      --offline               - Install only from local "
               "download cache"
            << std::endl;
  std::cout
      << "      --frozen-lockfile       - Fail if lockfile requires updating"
      << std::endl;
  std::cout << "      --registry <url>        - Override central registry "
               "endpoint URL"
            << std::endl;
  std::cout << std::endl;
  std::cout << "  " << termcolor::red << "topaz uninstall <name> [options]"
            << termcolor::reset << " - Remove a plugin" << std::endl;
  std::cout << "    Options:" << std::endl;
  std::cout << "      --local                 - Remove local copy only"
            << std::endl;
  std::cout << "      --global                - Remove global copy only"
            << std::endl;
  std::cout << std::endl;
  std::cout << "  " << termcolor::yellow << "topaz update [name] [options]"
            << termcolor::reset << "  - Check/update installed plugins"
            << std::endl;
  std::cout << "    Options:" << std::endl;
  std::cout << "      --check-only            - Only list available updates "
               "without installing"
            << std::endl;
  std::cout << std::endl;
  std::cout << "  " << termcolor::blue << "topaz list" << termcolor::reset
            << "                   - List installed plugins + runtime info"
            << std::endl;
  std::cout << "  " << termcolor::blue << "topaz info <name>"
            << termcolor::reset
            << "              - Show detailed information about a plugin"
            << std::endl;
  std::cout << "  " << termcolor::magenta << "topaz search <query>"
            << termcolor::reset
            << "         - Search for plugins in the registry" << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz check" << termcolor::reset
            << "                  - Run diagnostic on installed plugins"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz outdated [name]"
            << termcolor::reset
            << "        - List plugins with newer versions available"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz lock <name> [version]"
            << termcolor::reset << "       - Generate lock file for a plugin"
            << std::endl;
  std::cout << std::endl;
  std::cout << "  " << termcolor::cyan
            << "topaz tree [name] [version] [options]" << termcolor::reset
            << " - Show dependency tree" << std::endl;
  std::cout << "    Options:" << std::endl;
  std::cout << "      --format <text|mermaid|json> - Graph tree output format"
            << std::endl;
  std::cout << std::endl;
  std::cout << "  " << termcolor::red << "topaz purge <name> [version]"
            << termcolor::reset << "  - Remove specific version(s) of a plugin"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz help" << termcolor::reset
            << "                   - Show this help message" << std::endl;
  std::cout << std::endl;

  std::cout << termcolor::bold << "Cache Commands:" << termcolor::reset
            << std::endl;
  std::cout << "  " << termcolor::yellow << "topaz cache dir"
            << termcolor::reset
            << "              - Show cache directory path and contents"
            << std::endl;
  std::cout << "  " << termcolor::red << "topaz cache clean [options]"
            << termcolor::reset << "      - Clean the download cache"
            << std::endl;
  std::cout << "    Options:" << std::endl;
  std::cout << "      -f, --force             - Avoid confirmation prompt"
            << std::endl;
  std::cout << std::endl;

  std::cout << termcolor::bold << termcolor::magenta
            << "Sapphire Runtime Commands:" << termcolor::reset << std::endl;
  std::cout << "  " << termcolor::green << "topaz sapphire list"
            << termcolor::reset
            << "            - List available Sapphire versions (remote)"
            << std::endl;
  std::cout << "  " << termcolor::green << "topaz sapphire install <version>"
            << termcolor::reset << "  - Install and activate a Sapphire release"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz sapphire use <version>"
            << termcolor::reset << "       - Switch to an installed version"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "topaz sapphire current"
            << termcolor::reset << "         - Show the active Sapphire version"
            << std::endl;
  std::cout << "  " << termcolor::blue << "topaz sapphire versions"
            << termcolor::reset << "        - List locally installed versions"
            << std::endl;
  std::cout << "  " << termcolor::red << "topaz sapphire uninstall <version>"
            << termcolor::reset << " - Remove an installed version"
            << std::endl;
  std::cout << std::endl;

  std::cout << termcolor::magenta
            << "Version Formatting (SemVer):" << termcolor::reset << std::endl;
  std::cout << "  " << termcolor::cyan << "latest" << termcolor::reset
            << "                   - Resolves to the highest available version"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "1.0.0" << termcolor::reset
            << "                    - Exact version match" << std::endl;
  std::cout << "  " << termcolor::cyan << "\"^1.0.0\"" << termcolor::reset
            << "                 - Compatible updates (same major version)"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "\">1.0.0\"" << termcolor::reset
            << " , " << termcolor::cyan << "\"<2.0.0\"" << termcolor::reset
            << "      - Greater than / Less than a specific version"
            << std::endl;
  std::cout << "  " << termcolor::cyan << "\">=1.0.0\"" << termcolor::reset
            << ", " << termcolor::cyan << "\"<=2.0.0\"" << termcolor::reset
            << "      - Greater or equal / Less or equal" << std::endl;
  std::cout << termcolor::red
            << "  * Always use quotes (\"\") for >, <, ^ to avoid terminal "
               "redirection!"
            << termcolor::reset << std::endl;
  std::cout << std::endl;

  std::cout << termcolor::yellow
            << "Note: When inside a Sapphire project (has main.sp or plugins/ "
               "folder),"
            << termcolor::reset << std::endl;
  std::cout << termcolor::yellow
            << "      plugins are installed locally in ./plugins/ by default."
            << termcolor::reset << std::endl;
  std::cout << termcolor::yellow
            << "      Use --global flag to install/uninstall globally."
            << termcolor::reset << std::endl;
}

} // namespace topaz

int main(int argc, char *argv[]) {
  if (argc < 2) {
    topaz::print_help();
    return 0;
  }

  std::string command = argv[1];
  std::filesystem::path working_dir = std::filesystem::current_path();

  // Parse arguments and options
  bool local_flag = false;
  bool global_flag = false;
  bool save_dev = false;
  bool no_save = false;
  bool offline = false;
  bool frozen_lockfile = false;
  bool check_only = false;
  bool force = false;
  bool yes = false;

  std::string registry_opt = "";
  std::string format_opt = "text";
  std::string name_opt = "";
  std::string author_opt = "";
  std::string desc_opt = "";
  std::string ver_opt = "";

  std::vector<std::string> clean_args;

  for (int i = 2; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--local") {
      local_flag = true;
    } else if (arg == "--global") {
      global_flag = true;
    } else if (arg == "--save-dev" || arg == "-D") {
      save_dev = true;
    } else if (arg == "--no-save") {
      no_save = true;
    } else if (arg == "--offline") {
      offline = true;
    } else if (arg == "--frozen-lockfile") {
      frozen_lockfile = true;
    } else if (arg == "--check-only") {
      check_only = true;
    } else if (arg == "--force" || arg == "-f") {
      force = true;
    } else if (arg == "--yes" || arg == "-y") {
      yes = true;
    } else if (arg == "--registry" && i + 1 < argc) {
      registry_opt = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      format_opt = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      name_opt = argv[++i];
    } else if (arg == "--author" && i + 1 < argc) {
      author_opt = argv[++i];
    } else if (arg == "--description" && i + 1 < argc) {
      desc_opt = argv[++i];
    } else if (arg == "--version" && i + 1 < argc) {
      ver_opt = argv[++i];
    } else {
      clean_args.push_back(arg);
    }
  }

  // Apply global options
  if (!registry_opt.empty()) {
    topaz::g_registry_url = registry_opt;
  }
  if (offline) {
    topaz::g_offline = true;
  }

  try {
    if (command == "help" || command == "--help" || command == "-h") {
      topaz::print_help();
      return 0;
    } else if (command == "sapphire") {
      return topaz::commands::cmd_sapphire_dispatch(argc, argv, 2);
    } else if (command == "init") {
      return topaz::commands::cmd_init(working_dir, name_opt, author_opt,
                                       desc_opt, ver_opt, yes);
    } else if (command == "test") {
      std::string test_dir = clean_args.empty() ? "" : clean_args[0];
      return topaz::commands::cmd_test(working_dir, test_dir);
    } else if (command == "clean") {
      return topaz::commands::cmd_clean(working_dir);
    } else if (command == "expand") {
      if (clean_args.empty()) {
        std::cerr << termcolor::red << "[!] Missing version argument"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz expand <version>" << std::endl;
        return 1;
      }
      return topaz::commands::cmd_expand(clean_args[0], working_dir);
    } else if (command == "install") {
      std::string plugin_name = "";
      std::string version = "latest";

      if (clean_args.size() >= 1) {
        plugin_name = clean_args[0];
      }
      if (clean_args.size() >= 2) {
        version = clean_args[1];
      }

      bool install_local = false;
      if (topaz::is_sapphire_project() && !global_flag) {
        install_local = true;
      }
      if (local_flag)
        install_local = true;
      if (global_flag)
        install_local = false;

      return topaz::commands::cmd_install(plugin_name, version, install_local,
                                          save_dev, no_save, frozen_lockfile);
    } else if (command == "uninstall") {
      if (clean_args.empty()) {
        std::cerr << termcolor::red << "[!] Missing plugin name"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz uninstall <name> [--local] [--global]"
                  << std::endl;
        return 1;
      }

      std::string uninstall_name = clean_args[0];
      bool uninstall_local_only = false;

      if (topaz::is_sapphire_project() && !global_flag) {
        uninstall_local_only = true;
      }
      if (local_flag)
        uninstall_local_only = true;
      if (global_flag)
        uninstall_local_only = false;

      return topaz::commands::cmd_uninstall(uninstall_name,
                                            uninstall_local_only);
    } else if (command == "update") {
      std::string update_name = clean_args.empty() ? "" : clean_args[0];
      return topaz::commands::cmd_update(update_name, check_only);
    } else if (command == "check") {
      return topaz::commands::cmd_check();
    } else if (command == "list") {
      return topaz::commands::cmd_list();
    } else if (command == "upgrade") {
      return topaz::commands::cmd_upgrade();
    } else if (command == "info") {
      if (clean_args.empty()) {
        std::cerr << termcolor::red << "[!] Missing plugin name"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz info <name>" << std::endl;
        return 1;
      }
      return topaz::commands::cmd_info(clean_args[0]);
    } else if (command == "search") {
      if (clean_args.empty()) {
        std::cerr << termcolor::red << "[!] Missing search query"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz search <query>" << std::endl;
        return 1;
      }
      return topaz::commands::cmd_search(clean_args[0]);
    } else if (command == "outdated") {
      std::string outdated_name = clean_args.empty() ? "" : clean_args[0];
      return topaz::commands::cmd_outdated(outdated_name);
    } else if (command == "cache") {
      if (clean_args.empty()) {
        std::cerr << termcolor::red << "[!] Missing cache subcommand"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz cache clean | topaz cache dir" << std::endl;
        return 1;
      }
      std::string cache_subcmd = clean_args[0];
      if (cache_subcmd == "clean") {
        return topaz::commands::cmd_cache_clean(force);
      } else if (cache_subcmd == "dir") {
        return topaz::commands::cmd_cache_dir();
      } else {
        std::cerr << termcolor::red << "[!] Unknown cache subcommand: '"
                  << cache_subcmd << "'" << termcolor::reset << std::endl;
        return 1;
      }
    } else if (command == "lock") {
      if (clean_args.empty()) {
        std::cerr << termcolor::red << "[!] Missing plugin name"
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz lock <name> [version]" << std::endl;
        return 1;
      }
      std::string lock_name = clean_args[0];
      std::string lock_version =
          clean_args.size() >= 2 ? clean_args[1] : "latest";
      return topaz::commands::cmd_lock_generate(lock_name, lock_version);
    } else if (command == "tree") {
      std::string tree_name = clean_args.empty() ? "" : clean_args[0];
      std::string tree_version =
          clean_args.size() >= 2 ? clean_args[1] : "latest";
      return topaz::commands::cmd_tree(tree_name, tree_version, format_opt);
    } else if (command == "purge") {
      if (clean_args.empty()) {
        std::cerr << termcolor::red << "[!] Missing plugin name."
                  << termcolor::reset << std::endl;
        std::cerr << "Usage: topaz purge <name> [version] [--local] [--global]"
                  << std::endl;
        return 1;
      }
      std::string purge_name = clean_args[0];
      std::string purge_version = clean_args.size() >= 2 ? clean_args[1] : "";

      return topaz::commands::cmd_purge(purge_name, purge_version, local_flag,
                                        global_flag);
    } else {
      std::cerr << termcolor::red << "[!] Unknown command: " << command
                << termcolor::reset << std::endl;
      std::cerr << "Run 'topaz help' for usage information" << std::endl;
      return 1;
    }
  } catch (const std::exception &e) {
    std::cerr << termcolor::red << "[!] Fatal error: " << e.what()
              << termcolor::reset << std::endl;
    return 1;
  }

  return 0;
}