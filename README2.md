# Sapphire Lang 💎

Welcome to **Sapphire v1.0.7**, the dynamic, high-performance, and visually-driven programming language. Sapphire is designed to tear down the boilerplate and let you build stunning graphical user interfaces and highly parallel computing tasks with minimal friction.

## Why Sapphire?

Sapphire is built with three core pillars in mind:
1. **Speed & Parallelism:** Under the hood, Sapphire runs a heavily optimized C++ Virtual Machine featuring a lightning-fast *Incremental Mark-and-Sweep* Garbage Collector capable of allocating and processing millions of objects in parallel without breaking a sweat.
2. **Beautiful UI Out-of-the-Box:** Why struggle with thousands of CSS classes and DOM trees? Sapphire comes with a completely cohesive, native UI syntax featuring Flexbox, Grids, Buttons, and Displays, all instantiated directly via code without namespaces.
3. **Painless Developer Experience:** Optional semicolons, implicit dynamic typing, direct string inference (`"Score: " + 10`), and Python-style auto-declarations (`x = 10`) make the syntax completely fluid.

---

## What's New in v1.0.7?

We listened to your feedback, and the v1.0.7 release brings massive architectural improvements and new features!

### 🌟 Language Features
- **Dynamic Syntax:** You no longer need `valueToString()`. The VM handles dynamic string concatenation automatically! Semicolons (`;`) are now completely optional. 
- **Variable Flexibility:** You can now create variables Python-style (`x = 10`) or use `var` to safely shadow existing names in the same scope (`var x = 10; var x = 20;`). 
- **Enums & Consts:** Real, native support for `enum GameState { Menu, Playing }` and strictly immutable `const` declarations.
- **For Loops:** Full support for `for` loops, from standard counting to complex logic omission (`for(;;)`).
- **Macros:** Added macros to auto-generate repetitive logic at compile time.
- **Advanced Math:** Native `Vec2D` and `Vec3D` types built straight into the core for physics and rendering.

### 🎨 The UI Revolution
We threw away the old UI implementation and rewrote it to be beautiful:
- **No More Namespaces:** You can now declare a `Button` or `Text` directly instead of typing `UI.Button`.
- **Flexbox & Grid:** A newly built Flexbox and Grid engine handles responsive application design natively.
- **Property Freedom:** UI Styles can be imported natively (`import`) and you can declare style properties in any order you want.
- **New Native Components:** Say hello to `Display`, `Menu`, and `MenuItem`.

### ⚡ Performance & Safety
- **Anti-BSOD Memory Tracking:** Your computer will no longer crash! Sapphire now tracks exact string capacities (`chars.capacity()`) and implements a 500MB fail-safe memory ceiling per thread. If the GC is overwhelmed by 10 million objects, the VM gracefully aborts instead of taking your OS down with it.
- **Incremental GC:** The Garbage Collector was rebuilt into an Incremental Mark-and-Sweep system, processing tasks significantly faster and keeping your UI thread frame-perfect.
- **Parallel Computing:** Fully implemented native multi-threading and parallelism across the CPU architecture.

### ⚙️ CLI & Tooling Ecosystem
- **Sapphire CLI Rebuilt:** Features a new REPL, integrated Project Initialization (`sapphire init`), and Static Typechecking on demand.
- **Spack is Back:** The Sapphire Package Manager (`spack.exe`) is back using Dogfooding with the `SpackConfig.txt` standard.
- **Mine GUI:** The `mine.exe` interactive graphical CLI has received a complete aesthetic overhaul.
- **Global Standard:** All error messages have been translated to English with enhanced pointing context for variables and illegal characters.

---

## Getting Started

To initialize a new Sapphire project, simply run:
```bash
sapphire init my_project
```

### Example: The Beauty of Sapphire
```javascript
import "styles.sp"

const Title = "Welcome to Sapphire"
var counter = 0

// Native Auto-Variables
window_width = 800
window_height = 600

// Clean, Namespace-free UI with Flexbox
var myButton = Button {
    text: Title + " (Clicks: " + counter + ")", // Auto inference!
    justify: "center",
    onClick: function() {
        counter++;
        print("Clicked!");
    }
}
```

Dive into the `CHANGELOG.md` to see the granular breakdown of this release. Enjoy coding with Sapphire! 💎
