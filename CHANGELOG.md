# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Spark v2.3.0 — Renamed from Mine:**
  - Package manager renamed from "mine" to "spark" (more thematic with Sapphire).
  - Repository moved to `https://github.com/foxzyt/sapphire-spark`.
  - All commands updated: `spark init`, `spark install`, `spark search`, etc.
  - `spark outdated [name]` — Shows plugins with newer versions available. Compares installed version against the latest GitHub release/tag/version directory.
  - `spark cache clean` — Cleans the download cache with size preview and confirmation prompt.
  - `spark cache dir` — Shows the cache directory path, contents (files/dirs with sizes), and total space used.
  - `spark lock <name> [version]` — Generates `spark.lock` and `CHECKSUMS.txt` for a plugin via dependency resolution. Displays locked dependencies with checksums.
  - `spark tree [name] [version]` — Shows plugin dependency trees with Unicode box-drawing characters. Detects circular dependencies, missing plugins, and lock file status. Supports showing tree for all plugins or a specific one.
  - `spark purge <name> [version] [--local] [--global]` — Removes specific version(s) of a plugin from local and/or global scopes. Without a version, removes all versions. Supports `--local` and `--global` flags to target specific scopes.
  - `spark check` now scans both global (AppData) and local (./plugins/) directories, and validates exact version requirements for dependencies.
- **Spark v2.2.0 — Sapphire Runtime Version Manager (`spark sapphire`):**
  - Novo subsistema de controle de versão dos executáveis da Sapphire integrado ao Mine.
  - `mine sapphire list` — Lista todas as versões disponíveis na branch `mine` do repositório `foxzyt/Sapphire` via GitHub Contents API.
  - `mine sapphire install <version>` — Baixa e instala os binários (`sapphire.exe`, `runner.exe`, `spack.exe`, `mine.exe`) de uma versão específica. Suporta constraints SemVer completas: `latest`, `1.0.6`, `^1.0`, `>=1.0.5`, `<2.0`, etc.
  - `mine sapphire use <version>` — Ativa uma versão já instalada, copiando os binários para o diretório raiz (`%APPDATA%\Sapphire\bin\`).
  - `mine sapphire current` — Exibe a versão ativa, o caminho de instalação e o status de cada binário.
  - `mine sapphire versions` — Lista todas as versões instaladas localmente com tamanho e indicação da ativa.
  - `mine sapphire uninstall <version>` — Remove uma versão instalada, com auto-switch para a versão mais recente restante.
  - Versão ativa persistida em `%APPDATA%\Sapphire\bin\.version`.
  - Cada versão da Sapphire instalada em `%APPDATA%\Sapphire\bin\v<version>\` (isolamento de versões).
- **`core/semver.hpp` — Módulo SemVer centralizado:**
  - Extração e refatoração das funções de comparação SemVer (`compare`, `satisfies`, `resolve_best`, `normalize`, `with_v`) para módulo reutilizável em `core/semver.hpp`.
  - `commands/install.hpp` agora usa aliases para `semver.hpp`, eliminando código duplicado.
  - O novo subsistema `sapphire_version.hpp` reutiliza o mesmo módulo SemVer para resolução de versões dos binários.
- **`mine list` melhorado:** Exibe banner de "Sapphire Runtime" com versão ativa e caminho de instalação antes da listagem de plugins.
- **Newton Plugin v1.0.0:** Complete rewrite of the pure-Sapphire 2D physics engine — advanced, production-ready, zero native code injection.
  - **Broad-Phase Spatial Hashing:** O(1) per-cell collision detection using a spatial grid, replacing the previous O(n²) brute force. Configurable cell size via `cellSize` world option.
  - **Accurate Narrow-Phase (SAT + Analytic):** Circle-vs-circle (analytic), box-vs-box (Separating Axis Theorem, 2 axes), circle-vs-box (closest-point projection) with proper contact-point and penetration depth.
  - **Coulomb Friction Impulse:** Tangential friction impulse clamped to the Coulomb cone (`|jT| ≤ μ·|jN|`), computed at the exact contact point including angular velocity contribution (radius vectors `rA`, `rB`).
  - **Rotational Dynamics:** Bodies carry `angle`, `angularVel`, `torque`, `inertia`, and `invInertia`. Integration updates both linear and angular state. Moment of inertia computed analytically per shape (disk: `½mr²`, rectangle: `m(w²+h²)/12`).
  - **Joints System:** `Newton.distanceJoint()` (spring: Hooke + damping) and `Newton.pinJoint()` (fixed anchor: spring to world point), both solved every sub-step.
  - **Sleep System:** Bodies automatically sleep when speed drops below `sleepThreshold` for `sleepTime` seconds. Sleeping bodies are excluded from integration and broad-phase. `Newton.wakeNear(world, x, y, r)` wakes all bodies within radius.
  - **Sub-Step Integration:** `subSteps` option splits each `newton_step()` call into multiple sub-steps for high-velocity bodies.
  - **Baumgarte Positional Correction:** Prevents sinking with configurable slop and percentage correction, applied every resolution iteration.
  - **Material Presets:** `Newton.RUBBER`, `Newton.WOOD`, `Newton.METAL`, `Newton.ICE`, `Newton.STONE` — each with tuned restitution, friction, and density.
  - **Body Removal:** `Newton.remove(world, body)` marks a body for removal; swept at the start of each `newton_step()`.
  - **Raycast:** `Newton.raycast(world, ox, oy, dx, dy, maxDist)` — analytic ray-circle and slab-method ray-AABB intersection.
  - **Layer Collision Mask:** Optional `layerMask` world option controls which layer pairs can collide.
  - **Rendering Integration:** `Newton.render(world, opts)` uses native `drawRect` SFML global. Includes optional `showContacts` debug overlay.
  - **Stats / Debug:** `Newton.stats(world)` returns a map with frame, body, sleeping, contact, and joint counts. `Newton.debug(world)` prints a formatted stats line.
- **Automated Test Suite (20 new tests):** Expanded test coverage to catch failures early in CI/CD.
  - **8 Module-based tests:**
    - `tests/module/test_math_module.sp` - Math.abs, Math.pow, Math.sqrt, Math.max, Math.min
    - `tests/module/test_math_advanced.sp` - floor, ceil, sin, cos, clamp, lerp
    - `tests/module/test_string_module.sp` - String.trim, toUpperCase, toLowerCase, length, contains, replace
    - `tests/module/test_io_module.sp` - writeFile, readFile, appendFile, deleteFile, exists
    - `tests/module/test_list_util_module.sp` - listCreate, listAppend, listGet, listSet, listContains, listRemoveAt
    - `tests/module/test_system_module.sp` - getOS, getCoreCount, clock, sleep
    - `tests/module/test_json_module.sp` - JSON.stringify, JSON.parse, roundtrip with nested objects
    - `tests/module/test_sqlite_module.sp` - SQLite open, execute, query
  - **12 General mechanics tests:**
    - `tests/test_arrow_functions.sp` - Arrow functions with single and block body
    - `tests/test_string_interpolation.sp` - f-strings with expressions and nesting
    - `tests/test_optional_chaining.sp` - ?. operator and ?? nullish coalescing combined
    - `tests/test_array_spread.sp` - Spread operator, multiple spreads, destructuring
    - `tests/test_shorthand_maps.sp` - Shorthand properties, mixed shorthand/explicit, class fields
    - `tests/test_inheritance_complex.sp` - Multi-level inheritance chain with method overriding
    - `tests/test_exceptions_complex.sp` - try/catch/finally with and without exception
    - `tests/test_ternary_complex.sp` - Nested ternary and ternary in function returns
    - `tests/test_for_loop_complex.sp` - C-style for, for-in, nested for loops
    - `tests/test_async_operations.sp` - spawn/join with multiple threads
    - `tests/test_bitwise_operations.sp` - AND, OR, XOR, shifts, switch with default
    - `tests/module/test_http_module.sp` - httpPing, httpGet (network-dependent)
- **GitHub Actions Test Workflow:** New `.github/workflows/test.yml` automatically runs all 40+ tests on every push to `development` and `main`, and on PRs to `main`. Provides clear PASS/FAIL/SKIP summary.
- **Mine Package Manager v2.1.0:** Major upgrade with new commands and features.
  - **`mine uninstall <name>` (Clean Removal):** Removes plugin folders (local and/or global) and cleans up lock files. Warns if the plugin is a dependency of other installed plugins and asks for confirmation before proceeding.
  - **`mine update [name]` (Smart Update):** Checks GitHub releases/tags API for the latest version of each installed plugin. Compares semantic versions, downloads the differential update, and updates the PLUGIN.txt metadata. Supports updating all plugins at once or a specific one.
  - **`mine install <name> --local` (Local Project Scope):** Plugins can now be installed into the project's `./plugins/` directory instead of globally. When inside a Sapphire project (detected by `main.sp`, `sapphire.json`, `.sapphire`, or `plugins/` directory), `mine install` defaults to local scope. Use `--global` to force global installation.
  - **`mine list` Scope Display:** Now shows both "Global Plugins (AppData)" and "Local Plugins (./plugins/)" sections, with separate counts for each.
  - **`mine info <name>` Scope Awareness:** Shows both global and local installation details, including lockfile presence, version list, and registry status.
  - **Version Input Fallback (Fixed):** When the user types `mine install <name>` without a version, it now defaults to `"latest"` (always fetching the latest stable tag). The parser now properly handles flag-like arguments that were previously causing "Version not found" errors.
  - **Local Plugin Resolution:** Updated `fs_utils.hpp` with `get_local_plugin_dir()`, `is_sapphire_project()`, `is_plugin_installed_local()`, `is_plugin_installed_anywhere()`, `get_best_plugin_dir()`, `get_plugin_base_dir()`, and `get_plugin_versions()` — all supporting local-first resolution with global fallback.
  - **Dependency-Aware Uninstall:** `is_plugin_required_by_others()` checks both local and global scopes before allowing removal, preventing accidental breakage.
- **Mine Package Manager v2.0.0:** Complete rewrite of the Mine plugin management system with modular architecture.
  - Interactive plugin initialization with `mine init` command
  - Version management with `mine expand <version>` command
  - Automatic dependency resolution and installation
  - Plugin registry integration with GitHub
  - Colored terminal output for better UX
  - Automatic dependency fetching via FetchContent (httplib, nlohmann/json, miniz, termcolor)
  - Support for local third-party libraries to speed up builds
  - Build gate option `-DBUILD_MINE=ON` to build Mine from root CMakeLists.txt
- **Mine Lockfile System:** Complete lockfile implementation for dependency management.
  - `mine.lock` files generated in plugin directories with SHA256 checksums
  - JSON-based lockfile format using nlohmann::json for robust parsing
  - Dependency tree tracking with direct dependencies for each locked package
  - Source tracking (registry vs local) for each dependency
  - Checksum verification for integrity validation
  - `LockedDependency` struct with name, version, checksum, source, and dependencies
  - `LockFile` class with add_dependency(), write(), read(), and verify() methods
  - Automatic lockfile generation after successful plugin installation
  - Lockfile path: `%APPDATA%/Sapphire/plugins/<plugin_name>/mine.lock`
- **Version-Aware Import System:** Complete implementation of versioned imports in Sapphire language.
  - New `TOKEN_AT` token for version specification syntax
  - Parser support for `import plugin@version` and `import plugin@latest` syntax
  - Lexer recognition of `@` character for version delimiting
  - VM automatic path resolution for plugin imports
  - Support for multiple plugin path resolution strategies:
    - `plugins/<name>/versions/v<version>/files/main.sp`
    - `../plugins/<name>/versions/v<version>/files/main.sp`
    - `../../plugins/<name>/versions/v<version>/files/main.sp`
    - `../<name>/versions/v<version>/files/main.sp`
  - Backward compatibility with traditional string literal imports: `import "path/to/module.sp"`
  - Import caching to prevent duplicate module loading
- **Dependency Conflict Resolution:** Intelligent version conflict detection and management.
  - `version_conflicts_` map tracking all required versions per plugin
  - `check_version_conflict()` method to detect version mismatches
  - `record_version_requirement()` method to track dependency requirements
  - Informative error messages showing conflicting versions
  - Support for multiple versions of the same plugin to coexist
  - Guidance for users to use version-specific imports when conflicts occur
  - DFS-based dependency resolution to prevent infinite loops
- **Intelligent Caching System:** Efficient download caching for plugin management.
  - Unique cache keys: `<plugin_name>_<version>.zip`
  - Cache directory: `%APPDATA%/Sapphire/plugins/.cache/`
  - Automatic cache hit detection before downloading
  - Unique extraction directories per version: `extracted_<name>_<version>`
  - Cache cleanup functionality with `cleanup_cache()`
  - Significant reduction in redundant downloads
- **Mine Command Architecture:** Modular command system for plugin management.
  - `cmd_install()` - Install plugins with dependency resolution
  - `cmd_list()` - List all installed plugins with versions
  - `cmd_check()` - Verify plugin integrity and dependencies
  - `cmd_info()` - Display detailed plugin information
  - `cmd_expand()` - Interactive plugin version creation
  - `cmd_init()` - Initialize new plugin projects
- **Core Infrastructure:** Complete core system for plugin management.
  - `DependencyResolver` class with DFS-based resolution algorithm
  - `download_and_extract_plugin()` with GitHub ZIP URL handling
  - `parse_dependencies_txt()` for DEPENDENCIES.txt parsing
  - `write_dependencies_txt()` for DEPENDENCIES.txt generation
  - `parse_plugin_txt()` for PLUGIN.txt metadata parsing
  - `get_plugin_dir()`, `get_cache_dir()`, `get_version_dir()` utilities
  - GitHub API integration for registry queries
  - SSL/TLS support via httplib for secure downloads
- **Infinitum Plugin v1.0.0:** NumPy-like library for Sapphire with comprehensive vector/matrix operations.
  - Vector creation: `zeros()`, `ones()`, `arange()`, `linspace()`
  - Math operations: `add()`, `sub()`, `mul()`, `div()`, `scale()`
  - Reductions: `sum()`, `mean()`, `max()`, `min()`
  - Statistics: `std()`, `variance()`
  - Matrix operations: `zeros_matrix()`, `ones_matrix()`, `identity()`, `dot()`
  - Shape manipulation: `reshape()`, `transpose()`, `flatten()`
  - Slicing: `slice()`, `filter()` (boolean indexing)
  - Advanced linear algebra: `matmul()`, `determinant()`, `inverse()`
  - Broadcasting: `add_scalar()`, `sub_scalar()`, `mul_scalar()`, `div_scalar()`
  - Random generation: `rand()`, `randn()`, `randint()`, `choice()`
  - Utilities: `abs()`, `pow()`, `sqrt_list()`, `sort()`, `reverse()`, `print_vector()`
- **Test Infrastructure:** Comprehensive test suite for plugin management.
  - `test_infinitum.sp` - Main test suite for Infinitum plugin
  - `test_infinitum_local.sp` - Local variant of Infinitum tests
  - `test_infinitum_import.sp` - Test script for version-aware imports
  - Demonstration of import syntax: `import infinitum@"1.0.0"`
  - Verification of automatic path resolution
  - Integration testing with mine.lock system

### Fixed
- **`get_plugin_versions()` path bug:** Function was looking at `base_dir/"versions"` instead of `base_dir/plugin_name/"versions"`, causing version lists to always return empty for locally installed plugins. Now properly resolves versions across both local and global scopes.
- **`spark check` scope bug:** Diagnostic was only scanning the global AppData directory, missing plugins installed in the local `./plugins/` project scope. Now scans both scopes and shows scope labels.
- **`spark check` version validation:** Previously only checked if a dependency's directory existed, not if the required version was actually available. Now validates exact version requirements and shows warnings for version mismatches.
- **`spark install` downloading entire repository:** When installing a specific version (e.g., `spark install infinitum 1.0.0`), the system was downloading the entire `main` branch ZIP containing ALL versions, leaving extra versions on disk. Now resolves the actual version tag first and downloads ONLY that specific version's tag ZIP.
- **Deprecated references renamed from "mine" to "spark":** Core utilities (`sapphire_cmd.hpp`, `sapphire_version.hpp`) updated to reference `spark` instead of `mine` in messages and paths.
- **Global Versioned Plugin Imports:** Versioned imports now also resolve plugins
  installed in `%APPDATA%/Sapphire/plugins`, matching Mine's global installation
  scope.
- **Array assignment semantics:** Dynamic array writes now support appending at the next index when assigning to `len(array)`, matching the behavior used by Infinitum vector helpers.
- **Parser compatibility:** Function declarations now accept optional explicit return-type annotations such as `function main() void {}` and `function foo() string {}` while preserving the newer no-return-type syntax.

## [1.0.9] - 2026-07-15

### Added
- **Iterators:** Added support for `for in`, `for of`, and `foreach` syntax to iterate over arrays, maps, and strings.
  ```sapphire
  foreach (var item in array) print(item);
  ```
- **C-Style For Loops:** Standard `for` loops for traditional index iteration.
  ```sapphire
  for (var i = 0; i < 5; i = i + 1) print(i);
  ```
- **JSON Module:** Modernized native JSON library using `nlohmann::json`. Allows reliable stringification and parsing of complex Sapphire `ObjMap` objects.
  ```sapphire
  var jsonStr = JSON.stringify({ name: "Bob" });
  var obj = JSON.parse('{"name":"Bob"}');
  ```
- **String Utilities:** Greatly expanded the native `String` module with standard manipulation functions.
  ```sapphire
  var lower = String.toLowerCase("HELLO");
  var trimmed = String.trim("   spacing   ");
  ```
- **Module Imports:** Added import caching to prevent modules from being evaluated multiple times upon repeated imports.
- **SQLite Support:** Native SQLite integration allowing database creation, query execution, and fetching results as Sapphire maps.
  ```sapphire
  var sql = SQLite(); var db = sql.open("data.db");
  ```
- **Arrow Functions:** Added support for arrow function syntax.
  ```sapphire
  var square = (x) => x * x;
  ```
- **String Interpolation:** Added support for f-strings.
  ```sapphire
  print f"Hello {name}, your score is {score}";
  ```
- **Optional Chaining:** Added the `?.` operator to safely access nested properties without crashing.
  ```sapphire
  var email = user?.profile?.email;
  ```
- **Nullish Coalescing:** Added the `??` operator to provide default values when an expression evaluates to `nil`.
  ```sapphire
  var config = userConfig ?? defaultConfig;
  ```
- **Destructuring:** Added support for array destructuring.
  ```sapphire
  var [x, y] = arr;
  ```
- **Spread/Rest Operator:** Added the `...` operator for arrays.
  ```sapphire
  var merged = [...arr1, 4, 5];
  ```
- **Shorthand Properties:** Added support for shorthand properties in map literals.
  ```sapphire
  var map = { theme, fontSize };
  ```
- **Class Fields:** Allow `var` and `const` keywords when defining class fields.
  ```sapphire
  class User { var name; const type = "admin"; }
  ```

### Changed
- **Dynamic Typing:** Radically simplified the language syntax. Explicit typing keywords (`int`, `bool`, `string`, `void`, `double`, `float`) have been entirely removed.
- **Variable Declarations:** Variables and constants now strictly require the explicit use of `var` or `const` keyword. Unintentionally assigning to an undeclared variable will now throw a `Runtime Error`.
- **Functions:** Functions no longer require a return type. All functions inherently return a value, and if no explicit `return` is used, they seamlessly return `nil`.

### Fixed
- **VM Garbage Collection:** Fixed memory corruption in `OP_CALL` and `OP_LOOP` where `step_gc()` could be called with an inconsistent stack state.
- **Property Access:** Fixed `OP_GET_PROPERTY` and `OP_SET_PROPERTY` causing variant access exceptions when interacting with `ObjMap` objects.
- Fixed parser expressions that incorrectly required a semicolon when evaluating an arrow function.
- Fixed nullish coalescing type check failure when the left-hand side is `nil`.
