# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
