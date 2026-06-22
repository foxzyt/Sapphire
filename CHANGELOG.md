# Changelog

All notable changes to the **Sapphire Language** will be documented in this file.

## [1.0.7] - 2026-06-21

### 🚀 Major Features
- **Redesigned Garbage Collector:** Completely rebuilt the traditional mark-and-sweep GC into a highly optimized, lightning-fast **Incremental Mark-and-Sweep** architecture.
- **Parallelism & Threading:** Added parallel execution capabilities, allowing heavy computations and allocations across multiple CPU threads.
- **Auto Type Inference & String Concatenation:** String concatenation no longer requires manual `valueToString()`. The VM dynamically parses and concatenates numbers, booleans, and strings (`"Score: " + 10`).
- **Implicit Variable Declaration:** Variables can now be dynamically initialized just by assigning a value (Python-style: `x = 10`), without needing the `var` keyword.
- **Variable Shadowing:** Variables can now be safely redeclared in the same scope using `var` (`var x = 10; var x = 20;`).
- **Enums & Consts:** Added robust, zero-cost `enum` implementations (compiled natively as instance hashmaps) and strictly enforced `const` variables.
- **For Loops:** Added full support for `for` loops, including nested structures and omitted clauses (`for (var i = 0; i < 10; i++)`).
- **Optional Semicolons:** Semicolons (`;`) at the end of statements are now fully optional, making the syntax cleaner and more modern.
- **Math Types:** Added native `Vec2D` and `Vec3D` types for robust math and physics calculations.
- **Macros Support:** Added full macro support to generate repetitive code dynamically at compile time.

### 🎨 UI & Styling Revolution
- **Completely Redesigned UI Syntax:** The UI declaration syntax was rebuilt from scratch to eliminate boilerplate and make declarations incredibly cohesive and easy.
- **Flexbox & Grid System:** Replaced the old layout algorithms with a robust **Flexbox** engine and a completely rewritten Grid system.
- **Direct Namespace Modules:** Removed the `UI.` namespace for native elements. You can now instantiate directly with `Button`, `Text`, etc., instead of `UI.Button`.
- **Property Order Independence:** Styles can now be declared with their property names in absolutely any order without breaking the compiler.
- **Style Imports:** Added support to dynamically import styles from other files using the `import` statement.
- **New UI Components:** Added `Display`, `Menu`, and `MenuItem` native UI nodes.
- **Button Justification:** Added text justification properties specifically for Button nodes.

### 🛠 Tooling & CLI
- **Sapphire CLI Overhaul:** The `sapphire` CLI was entirely rebuilt! Now features project initialization commands, a live REPL, and static type-checking out of the box.
- **Mine CLI Facelift:** The `mine.exe` interface was completely redesigned to be visually stunning and more user-friendly.
- **Spack (Dogfooding):** Re-added the Sapphire Package Manager (`spack.exe`) powered by its own `SpackConfig.txt` standard. 

### 🛡 Stability & Developer Experience (DX)
- **Anti-BSOD Memory Protection:** Added a strict 500MB memory ceiling per thread to the VM. The GC now accurately tracks string capacities (`chars.capacity()`), preventing catastrophic memory leaks that previously crashed the OS (BSOD/Screen Tearing).
- **English Error Translation:** All compiler and runtime errors were translated to English for global standard compliance.
- **Improved Error Handling:** Error messages now correctly point to unexpected characters, undefined variables, and illegal assignments with greater accuracy.
- **Runtime Safeties:** The VM now strictly catches uninitialized variable reads (e.g., trying to read `y` before `y = 10` is executed) and blocks `const` mutations.
