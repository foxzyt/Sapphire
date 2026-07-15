# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.9] - 2026-07-15

### Added
- **Arrow Functions:** Added support for arrow function syntax `(x) => x * x;`.
- **String Interpolation:** Added support for f-strings `f"Hello {name}"`.
- **Optional Chaining:** Added the `?.` operator to safely access nested properties (`user?.profile?.email`).
- **Nullish Coalescing:** Added the `??` operator to provide default values when an expression evaluates to `nil` (`a ?? b`).
- **Destructuring:** Added support for array destructuring `var [x, y] = arr;`.
- **Spread/Rest Operator:** Added the `...` operator for arrays (`[...arr1, 4, 5]`).
- **Shorthand Properties:** Added support for shorthand properties in map literals (`{ theme, fontSize }`).
- **Class Fields:** Allow `var` and `const` keywords when defining class fields.

### Changed
- **Dynamic Typing:** Radically simplified the language syntax. Explicit typing keywords (`int`, `bool`, `string`, `void`, `double`, `float`) have been entirely removed.
- **Variable Declarations:** Variables and constants now strictly require the explicit use of `var` or `const` keyword. Unintentionally assigning to an undeclared variable will now throw a `Runtime Error`.
- **Functions:** Functions no longer require a return type. All functions inherently return a value, and if no explicit `return` is used, they seamlessly return `nil`.

### Fixed
- Fixed parser expressions that incorrectly required a semicolon when evaluating an arrow function.
- Fixed nullish coalescing type check failure when the left-hand side is `nil`.
