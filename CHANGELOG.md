# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog,
and this project adheres to Semantic Versioning.

[Unreleased]
Added
Core VM
Added specific error types: SyntaxError, RuntimeError, TypeError, ReferenceError, RangeError, NetworkError, DatabaseError, UIError, InternalError.

Added runtime detection of stack overflows and underflows. The VM inspects the call stack to identify the function active at the time of failure.

Added OP_FINALLY opcode and basic support for finally blocks.

Added assert(condition, message) native function for test assertions.

Added support for for in, for of, and foreach over arrays, maps, and strings.

Added C-style for loops.

Added arrow function syntax: (x) => x * x.

Added f-string interpolation.

Added optional chaining (?.).

Added nullish coalescing (??).

Added array destructuring.

Added spread/rest operator for arrays.

Added shorthand properties in map literals.

Added var and const keywords for class fields.

Added module import caching to avoid repeated evaluation.

Added version-aware import syntax: import plugin@version and import plugin@latest.

Added import scope prefixes: local:plugin@version, global:plugin@version, path:....

Added custom entry point routing via main: or entry: in PLUGIN.txt or sapphire.json.

Added FFI via libffi for dynamic calls with flexible typing (ffiCall).

Added experimental opcodes and base support for the Rubellite JIT VM. Disabled by default; bytecode interpreter remains the stable default.

Added preliminary OpenCL API (opencl_api.cpp).

Added database connectors for SQLite, MySQL, and PostgreSQL.

Added Crypto module: SHA-256, SHA-1, MD5, HMAC-SHA256, Base64 encode/decode, CSPRNG (randomBytes, randomHex), UUID v4, AES-256-CBC encrypt/decrypt.

Added Net module: TCP connect/send/receive/close, DNS resolution, local IP, port check.

Added advanced IO functions: listDir, listDirRecursive, copyFile, moveFile, rename, makeDir, makeAllDirs, getTempDir, readLines, readBinary, writeBinary, getAbsolutePath, getParentDir, getExtension, getBasename, isFile.

Added JSON module modernization using nlohmann::json for reliable stringification and parsing of ObjMap.

Added expanded String utilities: toLowerCase, trim, etc.

Added native SQLite integration for database creation, query execution, and fetching results as Sapphire maps.

Added UI components: Card, Badge, Tag, Stepper, Spinner, Notify.

Added global gradient properties: gradientFrom, gradientTo, gradientDir.

Added global shadow properties: shadowBlur, shadowOffsetX, shadowOffsetY, shadowColor.

Added glowColor for hover halo on Card and Button.

Added easing functions: ease-in-quad, ease-out-quad, ease-in-out-cubic, ease-out-bounce, ease-out-elastic.

Added functional ease-out-bounce and ease-out-elastic for keyframes.

Compiler, Parser, and Lexer
Added finally block parsing in try/catch/finally.

Added missing lexer keywords: within, every, undo, fade, fallback, linear, exponential.

Added optional explicit return-type annotations: function main() void {}, function foo() string {}.

Added disassembly support for OP_WITHIN_START, OP_WITHIN_END, OP_EVERY_TICK, OP_UNDO, OP_DEFINE_FADE.

Added unit scaling (s to milliseconds) for within, every, and fade blocks.

Added structured syntax error hints in error_hints.cpp with over 40 diagnostic messages.

Tooling
Added Citrine linter: static analysis with 200 manual checks across syntax, style, performance, security, and architecture. Supports interactive fix, undo with hidden cache files on Windows, explain mode, JSON report export, health score dashboard, inline rule suppression via citrine-disable-next-line, and CLI filters by category (--category) and severity (--level).

Added Garnet test runner: parallel test execution, recursive discovery of test_*.sp and *_test.sp, isolated threads, per-test timing, stack traces on assertion failures, lifecycle hooks (beforeAll, afterAll, beforeEach, afterEach), native assertion library (assertEquals, assertNotEquals, assertTrue, assertFalse, assertNull, assertNotNull, assertThrows), per-thread assertion counts, retries (--retries), name filtering (--filter), and JSON report export (--output).

Added Amethyst code formatter: format subcommand (4-space indentation, keyword spacing, trailing whitespace removal, blank-line collapsing, in-place writes) and check subcommand (validation without modification, non-zero exit code for CI/CD).

Added Quartz micro-benchmarker: 50 benchmarks covering VM core, stdlib, algorithms, concurrency, math, and hardware.

Integrated Citrine, Garnet, Amethyst, and Quartz into the main Sapphire help menu (sapphire -h / display_info()) under Tool Options.

Added sapphire test [file_or_dir] CLI command to scan and execute functions/methods starting with test or should.

Added sapphire lint <file> CLI command for static checks: non-PascalCase class names, function naming conventions, TODOs, line length, and syntax compilation validation.

Added topaz init with interactive bypass (-y / --yes) and direct flags (--name, --version, --author, --description).

Added project-level lockfiles (topaz.lock) for deterministic transitive dependency resolution.

Added advanced topaz install parameters: --save-dev / -D, --no-save, --offline, --frozen-lockfile, --registry <url>.

Added topaz tree [name] [version] --format json and --format mermaid.

Added topaz update [name] --check-only.

Added topaz cache clean --force / -f.

Added topaz outdated [name], topaz cache clean, topaz cache dir, topaz lock <name> [version], topaz purge <name> [version] [--local] [--global].

Added topaz sapphire runtime version manager: list, install <version>, use <version>, current, versions, uninstall <version>. Supports SemVer constraints (latest, 1.0.6, ^1.0, >=1.0.5, <2.0).

Added core/semver.hpp centralized SemVer module with compare, satisfies, resolve_best, normalize, with_v.

Added local plugin scope: topaz install <name> --local installs to ./plugins/; --global forces global scope.

Added topaz uninstall <name> with dependency-aware confirmation.

Added topaz update [name] for differential updates from GitHub releases/tags.

Added topaz list scope display for global and local plugins.

Added topaz info <name> scope awareness.

Added lockfile system: topaz.lock with SHA256 checksums, JSON format, dependency tree tracking, source tracking, and integrity verification.

Added dependency conflict resolution: version conflict detection, requirement tracking, and informative errors.

Added intelligent download caching: unique cache keys, cache hit detection, version-specific extraction directories, and cleanup.

Added TopazTestConfig.txt support for topaz test, specifying expected console output, regex, and assertions.

Added C++ Plugin API documentation at site/docs_plugins.html.

LSP
Added full keyword coverage: all Sapphire keywords as completion items with snippet templates.

Added full built-in function coverage: ~60 VM native functions with parameter hints and Markdown documentation.

Added type annotation coverage: int, bool, string, double, float, void.

Added UI component coverage: 30+ UI functions (Render, Button, Text, Grid, Flex, Slider, Input, Checkbox, ToggleSwitch, ComboBox, DataGrid, Canvas, etc.).

Added real-time diagnostics via textDocument/publishDiagnostics using the actual Sapphire compiler.

Added hover documentation for keywords, built-ins, types, and UI functions.

Added textDocument/signatureHelp with parameter hints triggered on ( and ,.

Added textDocument/documentSymbol, documentHighlight, definition, references, rename, formatting, foldingRange, and semanticTokens/full.

Added TextMate grammar rewrite: block comments, f-string interpolation, decorators, class/function captures, PascalCase type detection, SCREAMING_SNAKE constant detection, compound operators, and distinct built-in scope.

Added helper infrastructure in main.cpp: get_doc_line, get_word_at, scan_document_symbols, get_signatures, get_active_parameter, get_call_name, build_semantic_tokens_data, build_folding_ranges, format_document.

Added VS Code extension updates: language-configuration.json, sapphire.tmLanguage.json, English comments, and version bumps.

Build and Platform
Added complete cross-platform support for Linux, macOS, and Windows.

Converted dependencies (SFML, OpenSSL, httplib, nlohmann/json, FreeType) to CMake FetchContent.

Added platform-specific library linking for macOS (Cocoa, IOKit, OpenGL) and Linux (pthread, dl, GL, X11, Xcursor, Xrandr, Xi).

Added compiler-specific optimizations for MSVC, GCC, and Clang.

Added platform-specific executable path resolution in runner.cpp.

Added platform-specific executable extensions in beryl/main.cpp.

Added conditional compilation of Windows PE-specific functions in beryl.cpp.

Added Linux file dialog support via zenity in vm.cpp.

Added platform-specific config directories in topaz.cpp.

Added platform-specific font loading and file dialogs in beryl/beryl_ui.cpp.

Added platform-specific update scripts in topaz/include/commands/upgrade.hpp.

Added platform-specific binary names and config directories in topaz/include/core/sapphire_version.hpp.

Added platform-specific plugin directories in topaz/include/core/fs_utils.hpp.

Added UTF-8 code page (SetConsoleOutputCP(CP_UTF8)) and virtual terminal processing on Windows in init_terminal.

Tests and CI
Added automated test suite with 20 new tests: 8 module-based tests and 12 general mechanics tests.

Added GitHub Actions workflow .github/workflows/test.yml to run all tests on push to development and main, and on PRs to main.

Changed
Renamed Spack to Beryl. Updated build paths (src/spack to src/beryl), executable footer signatures (SPACK_V1/SPACK_V2 to BERYL_V1/BERYL_V2), config file (SpackConfig.txt to BerylConfig.txt), UI, and documentation.

Renamed Spark and Mine to Topaz. Updated package manager references, directories, commands, and repository to https://github.com/foxzyt/sapphire-topaz.

Renamed Sapphire VM to Corundum.

Replaced std::variant with a tagged union (SapphireValue) in the VM. Inlined frequently used checks and added fast paths for math operators (OP_ADD, OP_LESS, etc.) and OP_CALL.

Simplified language syntax: removed explicit typing keywords (int, bool, string, void, double, float).

Variables and constants now require explicit var or const. Assigning to an undeclared variable throws a RuntimeError.

Functions no longer require a return type. Functions return nil if no explicit return is used.

Simplified error messages: removed error codes, colors, and excessive context. Errors now include basic message and location.

Reduced Topaz output verbosity. Installation output refactored to be concise. Added --verbose / -v for detailed logs.

Rubellite JIT VM (USE_RUBELLITE) disabled by default in CMakeLists.txt. Bytecode interpreter is the default stable engine.

topaz install now resolves the exact version tag first and downloads only that version's ZIP instead of the entire repository.

topaz check now scans both global (AppData) and local (./plugins/) scopes and validates exact version requirements.

topaz list silently skips malformed plugin directories.

topaz search uses the GitHub API contents endpoint instead of a dead registry.json.

topaz purge checks dependency requirements and prompts for confirmation before removal.

topaz sapphire use stages overwrites using .old files to avoid access-denied errors on Windows. Updated spack.exe references to beryl.exe.

topaz.lock writes are atomic via std::filesystem::rename to prevent corruption.

Added install rollback: topaz install atomically rolls back sapphire.json and topaz.lock on failure.

Added SHA256 checksum verification during plugin downloads and unzipping.

Updated resolver.hpp to fall back to versions/ subdirectory when PLUGIN.txt is absent from root.

Updated get_plugin_versions() to resolve versions across local and global scopes.

Updated fs_utils.hpp with local-first plugin resolution helpers: get_local_plugin_dir(), is_sapphire_project(), is_plugin_installed_local(), is_plugin_installed_anywhere(), get_best_plugin_dir(), get_plugin_base_dir(), get_plugin_versions().

Updated is_plugin_required_by_others() to check both local and global scopes.

Updated array assignment semantics: dynamic array writes support appending at len(array).

Updated parser compatibility: function declarations accept optional explicit return-type annotations.

Updated miniz compilation: CMakeLists.txt compiles C files with C++ to avoid undefined references.

Updated VS Code extension package.json to v1.2.0.

Updated LSP debug log messages from Portuguese to English.

Fixed
Fixed class initialization (OBJ_CLASS) not invoking init() when arguments were passed, allowing class construction with arguments to work correctly.

Fixed VM stack corruption: OBJ_CLASS calls now throw correctly when arguments are provided to a default empty init, avoiding std::runtime_error crashes and stack leaks.

Fixed critical memory leak and stack corruption where constructors without internal state improperly consumed initialization arguments from the stack, causing uncontrolled stack advancement.

Fixed OP_CALL and OP_LOOP memory corruption where step_gc() could be called with an inconsistent stack state.

Fixed OP_GET_PROPERTY and OP_SET_PROPERTY variant access exceptions with ObjMap.

Fixed GC mark_value: OBJ_ARRAY check was unreachable; array child values were never marked.

Fixed GC blacken_object: added missing cases for OBJ_MAP, OBJ_ARRAY, OBJ_LRU, OBJ_PROMISE, OBJ_NAMED_ARG, OBJ_FADE.

Fixed GC mark_roots: added marking of event_loop_queue and current_promise.

Fixed native_io_exists to use std::filesystem::exists() instead of opening an ifstream.

Fixed adaptive GC threshold: max(1MB, bytes_allocated * 1.5) instead of * 2.

Fixed Rubellite JIT constant folding bounds guard for optimized_code[i-3].

Fixed Rubellite JIT labels[offset + N] accesses to check array bounds before emitting jumps.

Fixed Rubellite JIT interpreter fallback when jit.finalize() fails.

Fixed Rubellite JIT OP_LOOP underflow guard in backward-jump offset arithmetic.

Fixed parser error "Expected expression" when using finally. Parser now accepts and processes finally blocks.

Fixed parser expressions incorrectly requiring a semicolon when evaluating an arrow function.

Fixed nullish coalescing type check failure when the left-hand side is nil.

Fixed Topaz scope issue that caused improper printing of available versions. Improved grammar in installation logs.

Fixed get_plugin_versions() path bug: looked at base_dir/"versions" instead of base_dir/plugin_name/"versions".

Fixed topaz check scope bug: only scanned global AppData directory.

Fixed topaz check version validation: now validates exact version requirements and warns on mismatches.

Fixed topaz install downloading entire repository when installing a specific version.

Fixed topaz.lock race condition with atomic renames.

Fixed SemVer constraint parsing for comma or space separated constraints.

Fixed package signature/integrity validation with SHA256.

Fixed plugin resolution fallback for missing PLUGIN.txt.

Fixed install rollback for failed installation steps.

Fixed topaz list disruptive error logs for malformed plugin directories.

Fixed topaz purge dependency protections.

Fixed topaz search dead registry.json reference.

Fixed topaz sapphire use access-denied errors on Windows.

Fixed topaz check missing/empty version field validation.

Fixed Mermaid graph rendering for isolated root nodes.

Fixed Miniz compilation linker error.

Fixed textboxes misaligned when dragged to the left edge (ui_retained.cpp).

Fixed fatal memory leak in Download (HTTP_DOWNLOAD) and Fetch (http.cpp) caused by httplib::Client.

Fixed mapHas() error and dictionary behavior with long strings due to collisions and incorrect offset usage.

Fixed UI engine debug std::cout spam in apply_animations_to_tree.

Fixed Quartz benchmark JSON syntax errors (hw_json_parse, hw_json_stringify) to use correct Map literal keys and stringification.

Fixed Quartz invalid try / undo loop benchmark (concur_undo_backup) to emit undo; within the try block.

Fixed Quartz benchmarks to remove invalid class constructors and corrected assert(false) capture in try/catch blocks.

Fixed topaz deprecated references from "mine" to "topaz" in core utilities.

Fixed global versioned plugin imports to resolve plugins installed in %APPDATA%/Sapphire/plugins.

