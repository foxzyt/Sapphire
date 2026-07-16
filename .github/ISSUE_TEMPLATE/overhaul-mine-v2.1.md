---
name: Overhaul Mine Plugin Manager
about: Professional package manager features for Mine
title: "Overhaul Mine Plugin Manager to look more like Cargo or npm"
labels: enhancement
assignees: foxzyt
---

## Description

The Mine Package Manager is due for a major upgrade to bring it closer to the standards set by modern package managers like Cargo (Rust) and npm (Node.js). This overhaul aims to add essential commands and capabilities that make package management professional, reliable, and developer-friendly.

## Proposed Changes

### 1. `mine uninstall <name>` — Clean removal
- Remove plugin folders from both local (`./plugins/`) and global (`%APPDATA%/Sapphire/plugins/`) scopes
- Automatically clean up lock files (`mine.lock`, `CHECKSUMS.txt`)
- Warn the user if the plugin is a dependency of another installed plugin (dependency-aware uninstall)
- Ask for confirmation before proceeding with removal
- Support `--local` and `--global` flags

### 2. `mine update [name]` — Smart update from GitHub
- Query the GitHub API (`/repos/owner/repo/releases/latest`) to fetch the latest version tag
- Fall back to the tags endpoint if the releases endpoint is unavailable
- Compare current installed version with the latest available version using semantic versioning
- If a newer version is found, download and extract the plugin, update `PLUGIN.txt` metadata
- Support updating all installed plugins at once (`mine update`) or a specific one (`mine update <name>`)
- Show a clear summary of updated, up-to-date, and failed plugins

### 3. Local project scope (like `node_modules`)
- When inside a Sapphire project (detected by presence of `main.sp`, `sapphire.json`, `.sapphire`, or `plugins/` directory), `mine install` should default to installing into `./plugins/` instead of globally
- Provide `--global` flag to force installation into AppData
- `mine list` should show both "Global Plugins (AppData)" and "Local Plugins (./plugins/)" sections with separate counts
- `mine info <name>` should display installation details for both scopes
- Create new utility functions: `get_local_plugin_dir()`, `is_sapphire_project()`, `is_plugin_installed_local()`, `is_plugin_installed_anywhere()`, `get_best_plugin_dir()`, `get_plugin_base_dir()`, `get_plugin_versions()`

### 4. Version input handling — Fallback to `latest`
- If the user types `mine install <name>` without a version, default to `"latest"` (fetch the latest stable tag from the registry)
- Properly parse flag-like arguments (e.g., `--local`, `--global`) so they are not mistaken for version strings
- Fix the `[!] Version not found` error that currently occurs when no version is provided

### 5. Dependency-aware operations
- Before uninstalling a plugin, check if it is required by any other installed plugin (both in local and global scopes)
- If it is, warn the user and ask for confirmation before proceeding
- Scan all `DEPENDENCIES.txt` files across all installed versions in both scopes

### 6. Documentation
- Update `CHANGELOG.md` with all new features, changes, and commands
- Document the local vs global scope behavior clearly

## Files Affected
- `src/mine/include/core/fs_utils.hpp` — Add local scope helpers
- `src/mine/include/core/resolver.hpp` — Add local scope support
- `src/mine/include/commands/install.hpp` — Accept `local_scope` parameter
- `src/mine/include/commands/uninstall.hpp` — **New file** for clean removal
- `src/mine/include/commands/update.hpp` — **New file** for smart update
- `src/mine/include/commands/list.hpp` — Show global and local scopes
- `src/mine/include/commands/info.hpp` — Show scope-aware details
- `src/mine/src/main.cpp` — Add `uninstall`, `update` commands, flag parsing
- `CHANGELOG.md` — Document all changes

## Expected Outcome
Mine becomes a professional-grade package manager with clean install/uninstall workflows, smart update capabilities, project-scoped dependencies (like `node_modules`), and robust error handling.