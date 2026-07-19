#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bytecode_io.h"
#include "compiler.h"
#include "compiler/debug.h"
#include "config.h"
#include "lexer.h"
#include "preprocessor/preprocessor.h"
#include "termcolor.h"
#include "tokens.h"
#include "utils.h"
#include "vm.h"

// Include httplib last to avoid Windows.h namespace pollution
#include "httplib.h"
#include "nlohmann/json.hpp" // Required for LSP communication

using json = nlohmann::json;

// ---------------------------------------------------------
// LANGUAGE SERVER PROTOCOL (LSP) INTEGRATION
// ---------------------------------------------------------

void send_lsp_message(const json &message) {
  std::string payload = message.dump();
  std::cout << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
  std::cout.flush(); // Critical: VS Code reads immediately from stdout
}

// Stores the current document content and URI for completion and diagnostics
std::string current_document_content;
std::string current_document_uri;

// -------------------------------------------------------
// LSP Completion Item Helpers
// kind values: 1=Text, 2=Method, 3=Function, 4=Constructor,
//              5=Field, 6=Variable, 7=Class, 14=Keyword, 15=Snippet
// -------------------------------------------------------

static json make_keyword(const std::string &label, const std::string &detail,
                         const std::string &insertText = "") {
  json item = {{"label", label}, {"kind", 14}, {"detail", detail}};
  if (!insertText.empty()) {
    item["insertText"] = insertText;
    item["insertTextFormat"] = 2; // Snippet
  }
  return item;
}

static json make_function(const std::string &label, const std::string &detail,
                          const std::string &insertText,
                          const std::string &doc = "") {
  json item = {{"label", label},
               {"kind", 3},
               {"detail", detail},
               {"insertText", insertText},
               {"insertTextFormat", 2}};
  if (!doc.empty()) {
    item["documentation"] = {{"kind", "markdown"}, {"value", doc}};
  }
  return item;
}

static json make_type(const std::string &label, const std::string &detail) {
  return {{"label", label}, {"kind", 7}, {"detail", detail}};
}

// Build the full list of completion items once (re-used on every request)
static json build_all_completion_items() {
  json items = json::array();

  // --- Keywords ---
  items.push_back(make_keyword("function", "Declare a function",
                               "function ${1:name}(${2:params}) {\n    $3\n}"));
  items.push_back(
      make_keyword("var", "Declare a variable", "var ${1:name} = ${2:value};"));
  items.push_back(make_keyword("const", "Declare a constant",
                               "const ${1:name} = ${2:value};"));
  items.push_back(
      make_keyword("if", "If conditional", "if (${1:condition}) {\n    $2\n}"));
  items.push_back(make_keyword("else", "Else branch", "else {\n    $1\n}"));
  items.push_back(make_keyword("while", "While loop",
                               "while (${1:condition}) {\n    $2\n}"));
  items.push_back(make_keyword(
      "for", "For loop", "for (${1:init}; ${2:cond}; ${3:step}) {\n    $4\n}"));
  items.push_back(
      make_keyword("foreach", "Foreach loop over iterable",
                   "foreach (${1:item} in ${2:collection}) {\n    $3\n}"));
  items.push_back(make_keyword("in", "'in' operator (foreach / for..in)"));
  items.push_back(make_keyword("of", "'of' operator (for..of)"));
  items.push_back(make_keyword("return", "Return a value from a function",
                               "return ${1:value};"));
  items.push_back(make_keyword("import", "Import a module or script",
                               "import \"${1:path}\";"));
  items.push_back(make_keyword(
      "class", "Declare a class",
      "class ${1:Name} {\n    function ${2:init}() {\n        $3\n    }\n}"));
  items.push_back(make_keyword("extends", "Extend (inherit) a class"));
  items.push_back(make_keyword("this", "Reference to the current instance"));
  items.push_back(make_keyword("super", "Reference to the parent class"));
  items.push_back(make_keyword("new", "Instantiate a class",
                               "new ${1:ClassName}(${2:args})"));
  items.push_back(make_keyword("true", "Boolean literal true"));
  items.push_back(make_keyword("false", "Boolean literal false"));
  items.push_back(make_keyword("nil", "Null / nil value"));
  items.push_back(make_keyword("null", "Alias for nil"));
  items.push_back(make_keyword("and", "Logical AND operator"));
  items.push_back(make_keyword("or", "Logical OR operator"));
  items.push_back(make_keyword("break", "Break out of a loop"));
  items.push_back(
      make_keyword("continue", "Continue to the next loop iteration"));
  items.push_back(
      make_keyword("switch", "Switch statement",
                   "switch (${1:value}) {\n    case ${2:val}:\n        $3\n    "
                   "    break;\n    default:\n        $4\n}"));
  items.push_back(make_keyword("case", "Case branch in a switch statement",
                               "case ${1:value}:\n    $2\n    break;"));
  items.push_back(make_keyword(
      "default", "Default branch in a switch statement", "default:\n    $1"));
  items.push_back(
      make_keyword("try", "Try block for error handling",
                   "try {\n    $1\n} catch (${2:err}) {\n    $3\n}"));
  items.push_back(make_keyword("catch", "Catch block for error handling",
                               "catch (${1:err}) {\n    $2\n}"));
  items.push_back(
      make_keyword("throw", "Throw an error / exception", "throw ${1:error};"));
  items.push_back(make_keyword("finally", "Finally block (always executes)",
                               "finally {\n    $1\n}"));
  items.push_back(
      make_keyword("async", "Declare an async function",
                   "async function ${1:name}(${2:params}) {\n    $3\n}"));
  items.push_back(
      make_keyword("await", "Await an async result", "await ${1:expr}"));
  items.push_back(make_keyword("spawn", "Spawn a concurrent task",
                               "spawn ${1:function}(${2:args});"));
  items.push_back(make_keyword("enum", "Declare an enum type",
                               "enum ${1:Name} {\n    ${2:VALUE}\n}"));
  items.push_back(make_keyword("print", "Print to standard output (keyword)",
                               "print ${1:value};"));

  // --- Type annotations ---
  items.push_back(make_type("int", "Integer type annotation"));
  items.push_back(make_type("bool", "Boolean type annotation"));
  items.push_back(make_type("string", "String type annotation"));
  items.push_back(
      make_type("double", "Double-precision float type annotation"));
  items.push_back(make_type("float", "Single-precision float type annotation"));
  items.push_back(make_type("void", "Void return type annotation"));

  // --- Core built-in functions ---
  items.push_back(make_function(
      "clock", "clock() -> number", "clock()",
      "Returns the elapsed time in seconds since the program started."));
  items.push_back(make_function(
      "parseDouble", "parseDouble(str) -> number", "parseDouble(${1:str})",
      "Parses a string and returns its numeric (double) value."));
  items.push_back(make_function(
      "valueToString", "valueToString(val) -> string",
      "valueToString(${1:val})",
      "Converts any Sapphire value to its string representation."));
  items.push_back(
      make_function("evaluate", "evaluate(code: string)", "evaluate(${1:code})",
                    "Evaluates a Sapphire code string at runtime."));
  items.push_back(
      make_function("len", "len(val) -> number", "len(${1:val})",
                    "Returns the length of a string, list, or map."));
  items.push_back(make_function(
      "createInstance", "createInstance(className: string) -> object",
      "createInstance(${1:className})",
      "Dynamically creates an instance of a named class."));

  // --- String built-ins ---
  items.push_back(
      make_function("stringCharAt", "stringCharAt(str, index) -> string",
                    "stringCharAt(${1:str}, ${2:index})",
                    "Returns the character at the given index of `str`."));
  items.push_back(make_function("stringLength", "stringLength(str) -> number",
                                "stringLength(${1:str})",
                                "Returns the length of `str`."));
  items.push_back(make_function(
      "stringSubstring", "stringSubstring(str, start, end) -> string",
      "stringSubstring(${1:str}, ${2:start}, ${3:end})",
      "Extracts a substring from `str` between `start` and `end` indices."));
  items.push_back(make_function(
      "stringSplit", "stringSplit(str, delim) -> list",
      "stringSplit(${1:str}, ${2:delim})",
      "Splits `str` by `delim` and returns a list of substrings."));
  items.push_back(
      make_function("stringReplace", "stringReplace(str, from, to) -> string",
                    "stringReplace(${1:str}, ${2:from}, ${3:to})",
                    "Replaces all occurrences of `from` in `str` with `to`."));
  items.push_back(make_function("stringToUpper", "stringToUpper(str) -> string",
                                "stringToUpper(${1:str})",
                                "Returns `str` converted to uppercase."));
  items.push_back(make_function("stringToLower", "stringToLower(str) -> string",
                                "stringToLower(${1:str})",
                                "Returns `str` converted to lowercase."));
  items.push_back(make_function(
      "stringTrim", "stringTrim(str) -> string", "stringTrim(${1:str})",
      "Removes leading and trailing whitespace from `str`."));
  items.push_back(make_function("stringContains",
                                "stringContains(str, substr) -> bool",
                                "stringContains(${1:str}, ${2:substr})",
                                "Returns true if `str` contains `substr`."));
  items.push_back(make_function("getQuote", "getQuote() -> string",
                                "getQuote()",
                                "Returns a random inspirational quote."));

  // --- I/O built-ins ---
  items.push_back(make_function("readLine", "readLine() -> string",
                                "readLine()",
                                "Reads a line of input from stdin."));
  items.push_back(make_function(
      "printColor", "printColor(text, color)",
      "printColor(${1:text}, ${2:color})",
      "Prints `text` to the console using the specified color name."));
  items.push_back(make_function("writeFile", "writeFile(path, content)",
                                "writeFile(${1:path}, ${2:content})",
                                "Writes `content` to the file at `path`."));
  items.push_back(make_function(
      "readFile", "readFile(path) -> string", "readFile(${1:path})",
      "Reads and returns the full contents of the file at `path`."));
  items.push_back(
      make_function("exists", "exists(path) -> bool", "exists(${1:path})",
                    "Returns true if the file or directory at `path` exists."));
  items.push_back(make_function("deleteFile", "deleteFile(path)",
                                "deleteFile(${1:path})",
                                "Deletes the file at `path`."));
  items.push_back(make_function("appendFile", "appendFile(path, content)",
                                "appendFile(${1:path}, ${2:content})",
                                "Appends `content` to the file at `path`."));
  items.push_back(make_function(
      "openFileDialog", "openFileDialog() -> string", "openFileDialog()",
      "Opens a native file dialog and returns the selected file path."));

  // --- Math built-ins ---
  items.push_back(make_function("sqrt", "sqrt(x) -> number", "sqrt(${1:x})",
                                "Returns the square root of `x`."));
  items.push_back(
      make_function("rand", "rand() -> number", "rand()",
                    "Returns a random number between 0.0 and 1.0."));
  items.push_back(make_function("abs", "abs(x) -> number", "abs(${1:x})",
                                "Returns the absolute value of `x`."));
  items.push_back(
      make_function("floor", "floor(x) -> number", "floor(${1:x})",
                    "Returns the largest integer less than or equal to `x`."));
  items.push_back(make_function(
      "ceil", "ceil(x) -> number", "ceil(${1:x})",
      "Returns the smallest integer greater than or equal to `x`."));
  items.push_back(make_function("sin", "sin(x) -> number", "sin(${1:x})",
                                "Returns the sine of `x` (in radians)."));
  items.push_back(make_function("cos", "cos(x) -> number", "cos(${1:x})",
                                "Returns the cosine of `x` (in radians)."));
  items.push_back(make_function("log", "log(x) -> number", "log(${1:x})",
                                "Returns the natural logarithm of `x`."));
  items.push_back(make_function(
      "pow", "pow(base, exp) -> number", "pow(${1:base}, ${2:exp})",
      "Returns `base` raised to the power of `exp`."));
  items.push_back(make_function("min", "min(a, b) -> number",
                                "min(${1:a}, ${2:b})",
                                "Returns the smaller of `a` and `b`."));
  items.push_back(make_function("max", "max(a, b) -> number",
                                "max(${1:a}, ${2:b})",
                                "Returns the larger of `a` and `b`."));
  items.push_back(make_function("clamp", "clamp(val, min, max) -> number",
                                "clamp(${1:val}, ${2:min}, ${3:max})",
                                "Clamps `val` between `min` and `max`."));
  items.push_back(make_function(
      "lerp", "lerp(a, b, t) -> number", "lerp(${1:a}, ${2:b}, ${3:t})",
      "Linearly interpolates between `a` and `b` by factor `t`."));

  // --- List built-ins ---
  items.push_back(make_function("lruCreate", "lruCreate(capacity) -> lru_cache",
                                "lruCreate(${1:capacity})",
                                "Creates a new LRU cache with the specified capacity."));
  items.push_back(make_function("lruGet", "lruGet(cache, key) -> value",
                                "lruGet(${1:cache}, ${2:key})",
                                "Gets a value from the LRU cache."));
  items.push_back(make_function("lruPut", "lruPut(cache, key, value) -> value",
                                "lruPut(${1:cache}, ${2:key}, ${3:value})",
                                "Puts a value into the LRU cache."));
  items.push_back(make_function("lruHas", "lruHas(cache, key) -> boolean",
                                "lruHas(${1:cache}, ${2:key})",
                                "Checks if a key exists in the LRU cache."));

  items.push_back(make_function("listCreate", "listCreate() -> list",
                                "listCreate()",
                                "Creates and returns a new empty list."));
  items.push_back(make_function("listAppend", "listAppend(list, value)",
                                "listAppend(${1:list}, ${2:value})",
                                "Appends `value` to `list`."));
  items.push_back(make_function("listGet", "listGet(list, index) -> any",
                                "listGet(${1:list}, ${2:index})",
                                "Returns the element at `index` in `list`."));
  items.push_back(
      make_function("listSet", "listSet(list, index, value)",
                    "listSet(${1:list}, ${2:index}, ${3:value})",
                    "Sets the element at `index` in `list` to `value`."));
  items.push_back(make_function("listLength", "listLength(list) -> number",
                                "listLength(${1:list})",
                                "Returns the number of elements in `list`."));
  items.push_back(make_function("listRemoveAt", "listRemoveAt(list, index)",
                                "listRemoveAt(${1:list}, ${2:index})",
                                "Removes the element at `index` from `list`."));
  items.push_back(make_function("listContains",
                                "listContains(list, value) -> bool",
                                "listContains(${1:list}, ${2:value})",
                                "Returns true if `list` contains `value`."));

  // --- System built-ins ---
  items.push_back(
      make_function("getEnv", "getEnv(name) -> string", "getEnv(${1:name})",
                    "Returns the value of the environment variable `name`."));
  items.push_back(make_function(
      "getOS", "getOS() -> string", "getOS()",
      "Returns the current operating system name (e.g. 'Windows', 'Linux')."));
  items.push_back(make_function("sleep", "sleep(ms: number)", "sleep(${1:ms})",
                                "Pauses execution for `ms` milliseconds."));
  items.push_back(make_function(
      "getClipboard", "getClipboard() -> string", "getClipboard()",
      "Returns the current contents of the system clipboard."));
  items.push_back(make_function(
      "exec", "exec(command: string) -> string", "exec(${1:command})",
      "Runs a shell command and returns its stdout output."));
  items.push_back(make_function("spawn", "spawn(fn) -> thread",
                                "spawn(${1:fn})",
                                "Spawns a concurrent task running `fn`."));
  items.push_back(make_function("join", "join(thread)", "join(${1:thread})",
                                "Waits for a spawned thread to finish."));
  items.push_back(make_function("getCoreCount", "getCoreCount() -> number",
                                "getCoreCount()",
                                "Returns the number of CPU cores available."));

  // --- Network built-ins ---
  items.push_back(make_function(
      "httpGet", "httpGet(url) -> string", "httpGet(${1:url})",
      "Sends an HTTP GET request to `url` and returns the response body."));
  items.push_back(
      make_function("httpPost", "httpPost(url, body) -> string",
                    "httpPost(${1:url}, ${2:body})",
                    "Sends an HTTP POST request with `body` to `url`."));
  items.push_back(make_function("httpPing", "httpPing(url) -> bool",
                                "httpPing(${1:url})",
                                "Returns true if `url` responds to a ping."));
  items.push_back(
      make_function("httpDownload", "httpDownload(url, dest)",
                    "httpDownload(${1:url}, ${2:dest})",
                    "Downloads the file at `url` and saves it to `dest`."));
  items.push_back(make_function(
      "httpServer", "httpServer(port, handler)",
      "httpServer(${1:port}, ${2:handler})",
      "Starts an HTTP server on `port`, calling `handler` for each request."));

  // --- Color / misc ---
  items.push_back(make_function(
      "hexToRGB", "hexToRGB(hex) -> list", "hexToRGB(${1:hex})",
      "Converts a hex color string (e.g. '#FF0000') to an [r, g, b] list."));
  items.push_back(
      make_function("checkCollision", "checkCollision(a, b) -> bool",
                    "checkCollision(${1:a}, ${2:b})",
                    "Returns true if two collision objects overlap."));

  // --- Debug built-ins ---
  items.push_back(make_function("printStack", "printStack()", "printStack()",
                                "Prints the current VM stack for debugging."));
  items.push_back(
      make_function("dumpGlobals", "dumpGlobals()", "dumpGlobals()",
                    "Dumps all global variables to stderr for debugging."));

  // --- UI Components (active only when var UI = 1) ---
  items.push_back(make_function("Render", "Render(element)",
                                "Render(${1:element})",
                                "Renders a UI element to the window."));
  items.push_back(make_function(
      "Style", "Style(props) -> element", "Style({${1:key}: ${2:value}})",
      "Applies style properties to the next UI element."));
  items.push_back(
      make_function("Flex", "Flex(props, children)",
                    "Flex({${1:direction}: ${2:row}}, ${3:children})",
                    "Flex layout container."));
  items.push_back(make_function("Button", "Button(text, onClick)",
                                "Button(${1:text}, ${2:onClick})",
                                "A clickable button."));
  items.push_back(make_function("Text", "Text(content)", "Text(${1:content})",
                                "A text display element."));
  items.push_back(make_function("Display", "Display(props)",
                                "Display(${1:props})", "A display container."));
  items.push_back(
      make_function("Checkbox", "Checkbox(label, checked, onChange)",
                    "Checkbox(${1:label}, ${2:checked}, ${3:onChange})",
                    "A checkbox UI control."));
  items.push_back(
      make_function("Slider", "Slider(value, min, max, onChange)",
                    "Slider(${1:value}, ${2:min}, ${3:max}, ${4:onChange})",
                    "A slider control."));
  items.push_back(make_function("Input", "Input(id, placeholder)",
                                "Input(${1:id}, ${2:placeholder})",
                                "A text input field."));
  items.push_back(make_function(
      "GetInputText", "GetInputText(id) -> string", "GetInputText(${1:id})",
      "Returns the current text of an Input field by id."));
  items.push_back(make_function("Separator", "Separator()", "Separator()",
                                "A horizontal separator line."));
  items.push_back(make_function("Menu", "Menu(label, items)",
                                "Menu(${1:label}, ${2:items})",
                                "A menu bar item."));
  items.push_back(make_function("MenuItem", "MenuItem(label, onClick)",
                                "MenuItem(${1:label}, ${2:onClick})",
                                "An item inside a Menu."));
  items.push_back(make_function("Grid", "Grid(cols, rows, children)",
                                "Grid(${1:cols}, ${2:rows}, ${3:children})",
                                "A grid layout container."));
  items.push_back(make_function("StackPanel",
                                "StackPanel(orientation, children)",
                                "StackPanel(${1:orientation}, ${2:children})",
                                "A stacking layout panel."));
  items.push_back(make_function("DockPanel", "DockPanel(children)",
                                "DockPanel(${1:children})",
                                "A dock-based layout panel."));
  items.push_back(make_function("WrapPanel", "WrapPanel(children)",
                                "WrapPanel(${1:children})",
                                "A wrapping flow layout panel."));
  items.push_back(make_function("ScrollView", "ScrollView(children)",
                                "ScrollView(${1:children})",
                                "A scrollable container."));
  items.push_back(make_function("Border", "Border(props, child)",
                                "Border(${1:props}, ${2:child})",
                                "A bordered container."));
  items.push_back(make_function("Image", "Image(path)", "Image(${1:path})",
                                "Displays an image from `path`."));
  items.push_back(make_function("ProgressBar", "ProgressBar(value, max)",
                                "ProgressBar(${1:value}, ${2:max})",
                                "A progress bar."));
  items.push_back(make_function(
      "RadioBox", "RadioBox(label, selected, onChange)",
      "RadioBox(${1:label}, ${2:selected}, ${3:onChange})", "A radio button."));
  items.push_back(make_function(
      "ToggleSwitch", "ToggleSwitch(checked, onChange)",
      "ToggleSwitch(${1:checked}, ${2:onChange})", "A toggle switch."));
  items.push_back(
      make_function("ComboBox", "ComboBox(items, selected, onChange)",
                    "ComboBox(${1:items}, ${2:selected}, ${3:onChange})",
                    "A dropdown combo box."));
  items.push_back(make_function("ListBox", "ListBox(items, onSelect)",
                                "ListBox(${1:items}, ${2:onSelect})",
                                "A list selection box."));
  items.push_back(make_function("PasswordBox", "PasswordBox(id, placeholder)",
                                "PasswordBox(${1:id}, ${2:placeholder})",
                                "A password input field."));
  items.push_back(make_function("Hyperlink", "Hyperlink(text, url)",
                                "Hyperlink(${1:text}, ${2:url})",
                                "A clickable hyperlink."));
  items.push_back(make_function("Expander", "Expander(header, content)",
                                "Expander(${1:header}, ${2:content})",
                                "A collapsible expander panel."));
  items.push_back(make_function("DataGrid", "DataGrid(data, columns)",
                                "DataGrid(${1:data}, ${2:columns})",
                                "A data grid table."));
  items.push_back(make_function("Canvas", "Canvas(width, height, drawFn)",
                                "Canvas(${1:width}, ${2:height}, ${3:drawFn})",
                                "A 2D drawing canvas."));
  items.push_back(make_function("Tooltip", "Tooltip(text, child)",
                                "Tooltip(${1:text}, ${2:child})",
                                "Shows a tooltip on hover."));
  items.push_back(make_function("Popup", "Popup(content)",
                                "Popup(${1:content})",
                                "A popup/overlay dialog."));
  items.push_back(make_function("Window", "Window(title, content)",
                                "Window(${1:title}, ${2:content})",
                                "Creates a sub-window."));
  items.push_back(make_function("Animate", "Animate(props)",
                                "Animate(${1:props})",
                                "Applies an animation to a UI element."));

  return items;
}

// -------------------------------------------------------
// Hover documentation map (label -> markdown doc string)
// -------------------------------------------------------
static const std::unordered_map<std::string, std::string> hover_docs = {
    {"function", "**keyword** `function`\n\nDeclares a named "
                 "function.\n\n```sapphire\nfunction greet(name) {\n    print "
                 "name;\n}\n```"},
    {"var", "**keyword** `var`\n\nDeclares a mutable "
            "variable.\n\n```sapphire\nvar x = 10;\n```"},
    {"const", "**keyword** `const`\n\nDeclares an immutable "
              "constant.\n\n```sapphire\nconst PI = 3.14;\n```"},
    {"if", "**keyword** `if`\n\nConditional branch."},
    {"else", "**keyword** `else`\n\nAlternate branch of an `if` statement."},
    {"while",
     "**keyword** `while`\n\nRepeats a block while the condition is true."},
    {"for", "**keyword** `for`\n\nIterates with an init/condition/step form."},
    {"foreach",
     "**keyword** `foreach`\n\nIterates over each element of a collection."},
    {"return",
     "**keyword** `return`\n\nReturns a value from the current function."},
    {"import", "**keyword** `import`\n\nImports another Sapphire script or "
               "module.\n\n```sapphire\nimport \"utils.sp\";\n```"},
    {"class", "**keyword** `class`\n\nDeclares a class."},
    {"extends", "**keyword** `extends`\n\nInherits from a parent class."},
    {"this", "**keyword** `this`\n\nReferences the current object instance."},
    {"super", "**keyword** `super`\n\nReferences the parent class."},
    {"true", "**literal** `true` â€” Boolean value true."},
    {"false", "**literal** `false` â€” Boolean value false."},
    {"nil", "**literal** `nil` â€” The null / absent value."},
    {"null", "**literal** `null` â€” Alias for `nil`."},
    {"and", "**operator** `and` â€” Logical AND."},
    {"or", "**operator** `or` â€” Logical OR."},
    {"break", "**keyword** `break` â€” Exits the innermost loop."},
    {"continue", "**keyword** `continue` â€” Skips to the next loop iteration."},
    {"try", "**keyword** `try` â€” Begins an error-handling block."},
    {"catch",
     "**keyword** `catch` â€” Handles an error thrown in a `try` block."},
    {"throw", "**keyword** `throw` â€” Raises an exception."},
    {"finally", "**keyword** `finally` â€” Always executes after a try/catch."},
    {"async", "**keyword** `async` â€” Marks a function as asynchronous."},
    {"await", "**keyword** `await` â€” Waits for an async result."},
    {"spawn", "**keyword** `spawn` â€” Spawns a concurrent task."},
    {"enum", "**keyword** `enum` â€” Declares an enumeration."},
    {"switch", "**keyword** `switch` â€” Multi-branch conditional."},
    {"int", "**type** `int` â€” Integer type annotation."},
    {"bool", "**type** `bool` â€” Boolean type annotation."},
    {"string", "**type** `string` â€” String type annotation."},
    {"double", "**type** `double` â€” Double-precision float type annotation."},
    {"float", "**type** `float` â€” Single-precision float type annotation."},
    {"void", "**type** `void` â€” Void return type annotation."},
    {"clock", "**built-in** `clock() -> number`\n\nReturns seconds elapsed "
              "since program start."},
    {"len", "**built-in** `len(val) -> number`\n\nReturns the length of a "
            "string, list, or map."},
    {"parseDouble", "**built-in** `parseDouble(str) -> number`\n\nParses a "
                    "string to a double."},
    {"valueToString", "**built-in** `valueToString(val) -> string`\n\nConverts "
                      "any value to a string."},
    {"evaluate", "**built-in** `evaluate(code)`\n\nEvaluates Sapphire code "
                 "from a string at runtime."},
    {"createInstance",
     "**built-in** `createInstance(name) -> object`\n\nDynamically creates an "
     "instance of a named class."},
    {"stringCharAt", "**built-in** `stringCharAt(str, index) -> "
                     "string`\n\nReturns the character at `index`."},
    {"stringLength", "**built-in** `stringLength(str) -> number`\n\nReturns "
                     "the string length."},
    {"stringSubstring", "**built-in** `stringSubstring(str, start, end) -> "
                        "string`\n\nExtracts a substring."},
    {"stringSplit",
     "**built-in** `stringSplit(str, delim) -> list`\n\nSplits a string."},
    {"stringReplace", "**built-in** `stringReplace(str, from, to) -> "
                      "string`\n\nReplaces substrings."},
    {"stringToUpper",
     "**built-in** `stringToUpper(str) -> string`\n\nConverts to uppercase."},
    {"stringToLower",
     "**built-in** `stringToLower(str) -> string`\n\nConverts to lowercase."},
    {"stringTrim",
     "**built-in** `stringTrim(str) -> string`\n\nTrims whitespace."},
    {"stringContains", "**built-in** `stringContains(str, sub) -> "
                       "bool`\n\nChecks if string contains substring."},
    {"readLine",
     "**built-in** `readLine() -> string`\n\nReads a line from stdin."},
    {"printColor", "**built-in** `printColor(text, color)`\n\nPrints colored "
                   "text to console."},
    {"writeFile",
     "**built-in** `writeFile(path, content)`\n\nWrites content to a file."},
    {"readFile",
     "**built-in** `readFile(path) -> string`\n\nReads a file as a string."},
    {"exists", "**built-in** `exists(path) -> bool`\n\nChecks if a file or "
               "directory exists."},
    {"deleteFile", "**built-in** `deleteFile(path)`\n\nDeletes a file."},
    {"appendFile",
     "**built-in** `appendFile(path, content)`\n\nAppends content to a file."},
    {"openFileDialog", "**built-in** `openFileDialog() -> string`\n\nOpens a "
                       "native file picker dialog."},
    {"sqrt", "**built-in** `sqrt(x) -> number`\n\nSquare root."},
    {"rand", "**built-in** `rand() -> number`\n\nRandom float [0,1)."},
    {"abs", "**built-in** `abs(x) -> number`\n\nAbsolute value."},
    {"floor", "**built-in** `floor(x) -> number`\n\nFloor."},
    {"ceil", "**built-in** `ceil(x) -> number`\n\nCeiling."},
    {"sin", "**built-in** `sin(x) -> number`\n\nSine (radians)."},
    {"cos", "**built-in** `cos(x) -> number`\n\nCosine (radians)."},
    {"log", "**built-in** `log(x) -> number`\n\nNatural logarithm."},
    {"pow", "**built-in** `pow(base, exp) -> number`\n\nExponentiation."},
    {"min", "**built-in** `min(a, b) -> number`\n\nMinimum of two values."},
    {"max", "**built-in** `max(a, b) -> number`\n\nMaximum of two values."},
    {"clamp",
     "**built-in** `clamp(val, min, max) -> number`\n\nClamps value to range."},
    {"lerp", "**built-in** `lerp(a, b, t) -> number`\n\nLinear interpolation."},
    {"lruCreate",
     "**built-in** `lruCreate(capacity) -> lru_cache`\n\nCreates a new LRU cache."},
    {"lruGet",
     "**built-in** `lruGet(cache, key) -> value`\n\nGets an item from the LRU cache."},
    {"lruPut",
     "**built-in** `lruPut(cache, key, value) -> value`\n\nPuts an item in the LRU cache."},
    {"lruHas",
     "**built-in** `lruHas(cache, key) -> boolean`\n\nChecks if an item is in the LRU cache."},
    {"listCreate",
     "**built-in** `listCreate() -> list`\n\nCreates a new empty list."},
    {"listAppend",
     "**built-in** `listAppend(list, val)`\n\nAppends value to list."},
    {"listGet",
     "**built-in** `listGet(list, i) -> any`\n\nGets element at index."},
    {"listSet",
     "**built-in** `listSet(list, i, val)`\n\nSets element at index."},
    {"listLength", "**built-in** `listLength(list) -> number`\n\nList length."},
    {"listRemoveAt",
     "**built-in** `listRemoveAt(list, i)`\n\nRemoves element at index."},
    {"listContains",
     "**built-in** `listContains(list, val) -> bool`\n\nChecks membership."},
    {"getEnv",
     "**built-in** `getEnv(name) -> string`\n\nReads an environment variable."},
    {"getOS", "**built-in** `getOS() -> string`\n\nReturns the OS name."},
    {"sleep", "**built-in** `sleep(ms)`\n\nSleeps for `ms` milliseconds."},
    {"getClipboard",
     "**built-in** `getClipboard() -> string`\n\nReads the system clipboard."},
    {"exec", "**built-in** `exec(cmd) -> string`\n\nRuns a shell command."},
    {"join",
     "**built-in** `join(thread)`\n\nWaits for a spawned thread to complete."},
    {"getCoreCount",
     "**built-in** `getCoreCount() -> number`\n\nNumber of CPU cores."},
    {"httpGet", "**built-in** `httpGet(url) -> string`\n\nHTTP GET request."},
    {"httpPost",
     "**built-in** `httpPost(url, body) -> string`\n\nHTTP POST request."},
    {"httpPing", "**built-in** `httpPing(url) -> bool`\n\nPings a URL."},
    {"httpDownload",
     "**built-in** `httpDownload(url, dest)`\n\nDownloads a file."},
    {"httpServer",
     "**built-in** `httpServer(port, handler)`\n\nStarts an HTTP server."},
    {"hexToRGB",
     "**built-in** `hexToRGB(hex) -> list`\n\nConverts hex color to [r,g,b]."},
    {"checkCollision", "**built-in** `checkCollision(a, b) -> bool`\n\nChecks "
                       "collision between two objects."},
    {"printStack",
     "**built-in** `printStack()`\n\nPrints the VM stack (debug)."},
    {"dumpGlobals",
     "**built-in** `dumpGlobals()`\n\nDumps all global variables (debug)."},
    // UI
    {"Render", "**UI** `Render(element)`\n\nRenders a UI element."},
    {"Style", "**UI** `Style(props) -> element`\n\nApplies style properties."},
    {"Flex", "**UI** `Flex(props, children)`\n\nFlex layout container."},
    {"Button", "**UI** `Button(text, onClick)`\n\nA clickable button."},
    {"Text", "**UI** `Text(content)`\n\nA text display element."},
    {"Display", "**UI** `Display(props)`\n\nA display container."},
    {"Checkbox", "**UI** `Checkbox(label, checked, onChange)`\n\nA checkbox."},
    {"Slider", "**UI** `Slider(value, min, max, onChange)`\n\nA slider."},
    {"Input", "**UI** `Input(id, placeholder)`\n\nA text input."},
    {"GetInputText",
     "**UI** `GetInputText(id) -> string`\n\nGets input text by id."},
    {"Grid", "**UI** `Grid(cols, rows, children)`\n\nGrid layout."},
    {"StackPanel",
     "**UI** `StackPanel(orientation, children)`\n\nStack layout."},
    {"DockPanel", "**UI** `DockPanel(children)`\n\nDock layout."},
    {"WrapPanel", "**UI** `WrapPanel(children)`\n\nWrap layout."},
    {"ScrollView", "**UI** `ScrollView(children)`\n\nScrollable area."},
    {"Border", "**UI** `Border(props, child)`\n\nBordered container."},
    {"Image", "**UI** `Image(path)`\n\nDisplay an image."},
    {"ProgressBar", "**UI** `ProgressBar(value, max)`\n\nProgress indicator."},
    {"RadioBox",
     "**UI** `RadioBox(label, selected, onChange)`\n\nRadio button."},
    {"ToggleSwitch",
     "**UI** `ToggleSwitch(checked, onChange)`\n\nToggle switch."},
    {"ComboBox", "**UI** `ComboBox(items, selected, onChange)`\n\nDropdown."},
    {"ListBox", "**UI** `ListBox(items, onSelect)`\n\nList selector."},
    {"PasswordBox", "**UI** `PasswordBox(id, placeholder)`\n\nPassword input."},
    {"Hyperlink", "**UI** `Hyperlink(text, url)`\n\nClickable link."},
    {"Expander", "**UI** `Expander(header, content)`\n\nCollapsible panel."},
    {"DataGrid", "**UI** `DataGrid(data, columns)`\n\nData table."},
    {"Canvas", "**UI** `Canvas(w, h, drawFn)`\n\n2D drawing canvas."},
    {"Tooltip", "**UI** `Tooltip(text, child)`\n\nHover tooltip."},
    {"Popup", "**UI** `Popup(content)`\n\nOverlay popup."},
    {"Window", "**UI** `Window(title, content)`\n\nSub-window."},
    {"Animate", "**UI** `Animate(props)`\n\nUI animation."},
    {"Menu", "**UI** `Menu(label, items)`\n\nMenu bar entry."},
    {"MenuItem", "**UI** `MenuItem(label, onClick)`\n\nMenu item."},
    {"Separator", "**UI** `Separator()`\n\nHorizontal divider."},
    {"getQuote",
     "**built-in** `getQuote() -> string`\n\nReturns a random quote."},
};

// -------------------------------------------------------
// Publish diagnostics by compiling the document content
// -------------------------------------------------------
static void publish_diagnostics(const std::string &uri,
                                const std::string &source) {
  json diagnostics = json::array();

  if (!source.empty()) {
    // Capture stderr output from the compiler to parse errors
    // We redirect via a pipe trick: replace cerr buffer temporarily
    std::ostringstream capture;
    std::streambuf *old_cerr = std::cerr.rdbuf(capture.rdbuf());

    VM vm;
    Preprocessor prep;
    std::string processed = source;
    try {
      processed = prep.process(source);
    } catch (...) {
    }

    ObjFunction *func = compile(&vm, processed);

    std::cerr.rdbuf(old_cerr); // Restore cerr
    std::string error_output = capture.str();

    if (func == nullptr && !error_output.empty()) {
      // Parse error lines from the compiler output.
      // Format: "[Line L:C] Error at 'X': message" or "[Line L:C] Error at end:
      // message"
      std::istringstream ss(error_output);
      std::string eline;
      while (std::getline(ss, eline)) {
        // Strip ANSI color codes (simple approach)
        std::string clean;
        bool in_escape = false;
        for (char ch : eline) {
          if (ch == '\033') {
            in_escape = true;
            continue;
          }
          if (in_escape) {
            if (ch == 'm')
              in_escape = false;
            continue;
          }
          clean += ch;
        }

        // Match "[Line L:C] Error ...: message"
        auto bracket = clean.find("[Line ");
        if (bracket == std::string::npos)
          continue;

        auto colon_pos = clean.find(':', bracket + 6);
        auto bracket_end = clean.find(']', bracket);
        if (colon_pos == std::string::npos || bracket_end == std::string::npos)
          continue;

        int line_num = 1, col_num = 1;
        try {
          line_num =
              std::stoi(clean.substr(bracket + 6, colon_pos - bracket - 6));
          col_num = std::stoi(
              clean.substr(colon_pos + 1, bracket_end - colon_pos - 1));
        } catch (...) {
          continue;
        }

        // Extract the human-readable message after the last ':'
        auto msg_colon = clean.rfind(':');
        std::string message = "Syntax error.";
        if (msg_colon != std::string::npos && msg_colon + 1 < clean.size()) {
          message = clean.substr(msg_colon + 1);
          // Trim leading spaces
          size_t start = message.find_first_not_of(' ');
          if (start != std::string::npos)
            message = message.substr(start);
        }

        if (message.empty())
          message = "Syntax error.";

        int lsp_line = std::max(0, line_num - 1); // LSP uses 0-based lines
        int lsp_col = std::max(0, col_num - 1);

        json diag = {
            {"range",
             {{"start", {{"line", lsp_line}, {"character", lsp_col}}},
              {"end", {{"line", lsp_line}, {"character", lsp_col + 1}}}}},
            {"severity", 1}, // 1 = Error
            {"source", "sapphire"},
            {"message", message}};
        diagnostics.push_back(diag);
      }
    }
  }

  json notification = {
      {"jsonrpc", "2.0"},
      {"method", "textDocument/publishDiagnostics"},
      {"params", {{"uri", uri}, {"diagnostics", diagnostics}}}};
  send_lsp_message(notification);
  std::cerr << "[Sapphire LSP] Published " << diagnostics.size()
            << " diagnostic(s) for " << uri << std::endl;
}

// =============================================================
// HELPER UTILITIES
// =============================================================

// Return a single line from the document (0-based index)
static std::string get_doc_line(const std::string &src, int target_line) {
  std::istringstream ss(src);
  std::string ln;
  int n = 0;
  while (std::getline(ss, ln)) {
    if (n == target_line) {
      if (!ln.empty() && ln.back() == '\r')
        ln.pop_back();
      return ln;
    }
    n++;
  }
  return "";
}

// Extract the word around a given (line, col) in the document
static std::string get_word_at(const std::string &src, int target_line,
                               int target_col) {
  std::string ln = get_doc_line(src, target_line);
  int limit = std::min(target_col, (int)ln.size());
  int ws = limit;
  while (ws > 0 && (isalnum((unsigned char)ln[ws - 1]) || ln[ws - 1] == '_'))
    ws--;
  int we = limit;
  while (we < (int)ln.size() &&
         (isalnum((unsigned char)ln[we]) || ln[we] == '_'))
    we++;
  return ln.substr(ws, we - ws);
}

// =============================================================
// DOCUMENT SYMBOL SCANNING
// Kind: 12=Function, 5=Class, 13=Variable, 10=Enum, 8=Field
// =============================================================
struct DocSymbol {
  std::string name;
  int kind; // LSP SymbolKind
  int line; // 0-based
  int col;
  int end_line;
  int end_col;
};

static std::vector<DocSymbol> scan_document_symbols(const std::string &src) {
  std::vector<DocSymbol> symbols;
  std::istringstream ss(src);
  std::string ln;
  int line_no = 0;

  // Simple regex-style scan using token patterns
  std::regex re_func(R"(\bfunction\s+([A-Za-z_][A-Za-z0-9_]*)\s*\()");
  std::regex re_class(R"(\bclass\s+([A-Za-z_][A-Za-z0-9_]*))");
  std::regex re_var(R"(\b(var|const)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=)");
  std::regex re_enum(R"(\benum\s+([A-Za-z_][A-Za-z0-9_]*))");

  while (std::getline(ss, ln)) {
    if (!ln.empty() && ln.back() == '\r')
      ln.pop_back();

    std::smatch m;
    if (std::regex_search(ln, m, re_func)) {
      DocSymbol sym;
      sym.name = m[1].str();
      sym.kind = 12; // Function
      sym.line = line_no;
      sym.col = (int)m.position(1);
      sym.end_line = line_no;
      sym.end_col = sym.col + (int)sym.name.size();
      symbols.push_back(sym);
    } else if (std::regex_search(ln, m, re_class)) {
      DocSymbol sym;
      sym.name = m[1].str();
      sym.kind = 5; // Class
      sym.line = line_no;
      sym.col = (int)m.position(1);
      sym.end_line = line_no;
      sym.end_col = sym.col + (int)sym.name.size();
      symbols.push_back(sym);
    } else if (std::regex_search(ln, m, re_enum)) {
      DocSymbol sym;
      sym.name = m[1].str();
      sym.kind = 10; // Enum
      sym.line = line_no;
      sym.col = (int)m.position(1);
      sym.end_line = line_no;
      sym.end_col = sym.col + (int)sym.name.size();
      symbols.push_back(sym);
    }
    // Scan all var/const declarations on this line
    std::string rest = ln;
    std::smatch mv;
    while (std::regex_search(rest, mv, re_var)) {
      DocSymbol sym;
      sym.name = mv[2].str();
      sym.kind = (mv[1].str() == "const") ? 14 : 13; // Constant or Variable
      // Compute absolute position
      std::string prefix =
          ln.substr(0, ln.size() - rest.size() + (int)mv.position(2));
      sym.col = (int)prefix.size();
      sym.line = line_no;
      sym.end_line = line_no;
      sym.end_col = sym.col + (int)sym.name.size();
      symbols.push_back(sym);
      rest = mv.suffix().str();
    }
    line_no++;
  }
  return symbols;
}

// =============================================================
// SIGNATURE HELP DATA
// =============================================================
struct SigParam {
  std::string label;
};
struct SigInfo {
  std::string label;
  std::vector<SigParam> params;
  std::string doc;
};

static const std::unordered_map<std::string, SigInfo> &get_signatures() {
  static const std::unordered_map<std::string, SigInfo> sigs = {
      // Core
      {"clock",
       {"clock()", {}, "Returns elapsed time in seconds since program start."}},
      {"parseDouble",
       {"parseDouble(str)", {{"str: string"}}, "Parse a string to a double."}},
      {"valueToString",
       {"valueToString(val)",
        {{"val: any"}},
        "Convert any value to its string representation."}},
      {"evaluate",
       {"evaluate(code)",
        {{"code: string"}},
        "Evaluate Sapphire code at runtime."}},
      {"len",
       {"len(val)",
        {{"val: string|list|map"}},
        "Return the length of a string, list, or map."}},
      {"createInstance",
       {"createInstance(className)",
        {{"className: string"}},
        "Dynamically create an instance of a named class."}},
      // Strings
      {"stringCharAt",
       {"stringCharAt(str, index)",
        {{"str: string"}, {"index: int"}},
        "Character at index."}},
      {"stringLength",
       {"stringLength(str)", {{"str: string"}}, "Length of string."}},
      {"stringSubstring",
       {"stringSubstring(str, start, end)",
        {{"str: string"}, {"start: int"}, {"end: int"}},
        "Extract substring."}},
      {"stringSplit",
       {"stringSplit(str, delim)",
        {{"str: string"}, {"delim: string"}},
        "Split by delimiter."}},
      {"stringReplace",
       {"stringReplace(str, from, to)",
        {{"str: string"}, {"from: string"}, {"to: string"}},
        "Replace all occurrences."}},
      {"stringToUpper",
       {"stringToUpper(str)", {{"str: string"}}, "To uppercase."}},
      {"stringToLower",
       {"stringToLower(str)", {{"str: string"}}, "To lowercase."}},
      {"stringTrim",
       {"stringTrim(str)", {{"str: string"}}, "Trim whitespace."}},
      {"stringContains",
       {"stringContains(str, substr)",
        {{"str: string"}, {"substr: string"}},
        "Check containment."}},
      {"getQuote", {"getQuote()", {}, "Return a random quote."}},
      // I/O
      {"readLine", {"readLine()", {}, "Read a line from stdin."}},
      {"printColor",
       {"printColor(text, color)",
        {{"text: string"}, {"color: string"}},
        "Print colored text."}},
      {"writeFile",
       {"writeFile(path, content)",
        {{"path: string"}, {"content: string"}},
        "Write to file."}},
      {"readFile",
       {"readFile(path)", {{"path: string"}}, "Read file as string."}},
      {"exists", {"exists(path)", {{"path: string"}}, "Check if path exists."}},
      {"deleteFile",
       {"deleteFile(path)", {{"path: string"}}, "Delete a file."}},
      {"appendFile",
       {"appendFile(path, content)",
        {{"path: string"}, {"content: string"}},
        "Append to file."}},
      {"openFileDialog", {"openFileDialog()", {}, "Open native file picker."}},
      // Math
      {"sqrt", {"sqrt(x)", {{"x: number"}}, "Square root."}},
      {"rand", {"rand()", {}, "Random float [0,1)."}},
      {"abs", {"abs(x)", {{"x: number"}}, "Absolute value."}},
      {"floor", {"floor(x)", {{"x: number"}}, "Floor."}},
      {"ceil", {"ceil(x)", {{"x: number"}}, "Ceiling."}},
      {"sin", {"sin(x)", {{"x: number"}}, "Sine (radians)."}},
      {"cos", {"cos(x)", {{"x: number"}}, "Cosine (radians)."}},
      {"log", {"log(x)", {{"x: number"}}, "Natural logarithm."}},
      {"pow",
       {"pow(base, exp)",
        {{"base: number"}, {"exp: number"}},
        "Exponentiation."}},
      {"min", {"min(a, b)", {{"a: number"}, {"b: number"}}, "Minimum."}},
      {"max", {"max(a, b)", {{"a: number"}, {"b: number"}}, "Maximum."}},
      {"clamp",
       {"clamp(val, min, max)",
        {{"val: number"}, {"min: number"}, {"max: number"}},
        "Clamp to range."}},
      {"lerp",
       {"lerp(a, b, t)",
        {{"a: number"}, {"b: number"}, {"t: number"}},
        "Linear interpolation."}},
      // Lists
      {"lruCreate", {"lruCreate(capacity)", {{"capacity: int"}}, "Create LRU cache."}},
      {"lruGet", {"lruGet(cache, key)", {{"cache: lru_cache"}, {"key: string"}}, "Get from LRU cache."}},
      {"lruPut", {"lruPut(cache, key, value)", {{"cache: lru_cache"}, {"key: string"}, {"value: any"}}, "Put in LRU cache."}},
      {"lruHas", {"lruHas(cache, key)", {{"cache: lru_cache"}, {"key: string"}}, "Check LRU cache."}},
      {"listCreate", {"listCreate()", {}, "Create empty list."}},
      {"listAppend",
       {"listAppend(list, value)",
        {{"list: list"}, {"value: any"}},
        "Append value."}},
      {"listGet",
       {"listGet(list, index)",
        {{"list: list"}, {"index: int"}},
        "Get element."}},
      {"listSet",
       {"listSet(list, index, value)",
        {{"list: list"}, {"index: int"}, {"value: any"}},
        "Set element."}},
      {"listLength", {"listLength(list)", {{"list: list"}}, "List length."}},
      {"listRemoveAt",
       {"listRemoveAt(list, index)",
        {{"list: list"}, {"index: int"}},
        "Remove element."}},
      {"listContains",
       {"listContains(list, value)",
        {{"list: list"}, {"value: any"}},
        "Check membership."}},
      // System
      {"getEnv",
       {"getEnv(name)", {{"name: string"}}, "Read environment variable."}},
      {"getOS", {"getOS()", {}, "Return OS name."}},
      {"sleep", {"sleep(ms)", {{"ms: number"}}, "Sleep milliseconds."}},
      {"getClipboard", {"getClipboard()", {}, "Read clipboard."}},
      {"exec", {"exec(command)", {{"command: string"}}, "Run shell command."}},
      {"spawn", {"spawn(fn)", {{"fn: function"}}, "Spawn thread."}},
      {"join", {"join(thread)", {{"thread: thread"}}, "Wait for thread."}},
      {"getCoreCount", {"getCoreCount()", {}, "CPU core count."}},
      // Network
      {"httpGet", {"httpGet(url)", {{"url: string"}}, "HTTP GET."}},
      {"httpPost",
       {"httpPost(url, body)",
        {{"url: string"}, {"body: string"}},
        "HTTP POST."}},
      {"httpPing", {"httpPing(url)", {{"url: string"}}, "Ping URL."}},
      {"httpDownload",
       {"httpDownload(url, dest)",
        {{"url: string"}, {"dest: string"}},
        "Download file."}},
      {"httpServer",
       {"httpServer(port, handler)",
        {{"port: int"}, {"handler: function"}},
        "Start HTTP server."}},
      // Color / misc
      {"hexToRGB",
       {"hexToRGB(hex)", {{"hex: string"}}, "Hex string to [r,g,b]."}},
      {"checkCollision",
       {"checkCollision(a, b)",
        {{"a: object"}, {"b: object"}},
        "Collision test."}},
      {"printStack", {"printStack()", {}, "Print VM stack (debug)."}},
      {"dumpGlobals", {"dumpGlobals()", {}, "Dump globals (debug)."}},
      // UI
      {"Render",
       {"Render(element)", {{"element: any"}}, "Render a UI element."}},
      {"Style", {"Style(props)", {{"props: map"}}, "Apply style properties."}},
      {"Flex",
       {"Flex(props, children)",
        {{"props: map"}, {"children: list"}},
        "Flex layout."}},
      {"Button",
       {"Button(text, onClick)",
        {{"text: string"}, {"onClick: function"}},
        "Clickable button."}},
      {"Text", {"Text(content)", {{"content: string"}}, "Text element."}},
      {"Display", {"Display(props)", {{"props: map"}}, "Display container."}},
      {"Checkbox",
       {"Checkbox(label, checked, onChange)",
        {{"label: string"}, {"checked: bool"}, {"onChange: function"}},
        "Checkbox."}},
      {"Slider",
       {"Slider(value, min, max, onChange)",
        {{"value: number"},
         {"min: number"},
         {"max: number"},
         {"onChange: function"}},
        "Slider."}},
      {"Input",
       {"Input(id, placeholder)",
        {{"id: string"}, {"placeholder: string"}},
        "Text input."}},
      {"GetInputText",
       {"GetInputText(id)", {{"id: string"}}, "Get input text."}},
      {"Separator", {"Separator()", {}, "Separator line."}},
      {"Menu",
       {"Menu(label, items)",
        {{"label: string"}, {"items: list"}},
        "Menu bar entry."}},
      {"MenuItem",
       {"MenuItem(label, onClick)",
        {{"label: string"}, {"onClick: function"}},
        "Menu item."}},
      {"Grid",
       {"Grid(cols, rows, children)",
        {{"cols: int"}, {"rows: int"}, {"children: list"}},
        "Grid layout."}},
      {"StackPanel",
       {"StackPanel(orientation, children)",
        {{"orientation: string"}, {"children: list"}},
        "Stack panel."}},
      {"DockPanel",
       {"DockPanel(children)", {{"children: list"}}, "Dock panel."}},
      {"WrapPanel",
       {"WrapPanel(children)", {{"children: list"}}, "Wrap panel."}},
      {"ScrollView",
       {"ScrollView(children)", {{"children: list"}}, "Scrollable area."}},
      {"Border",
       {"Border(props, child)",
        {{"props: map"}, {"child: any"}},
        "Bordered container."}},
      {"Image", {"Image(path)", {{"path: string"}}, "Display image."}},
      {"ProgressBar",
       {"ProgressBar(value, max)",
        {{"value: number"}, {"max: number"}},
        "Progress bar."}},
      {"RadioBox",
       {"RadioBox(label, selected, onChange)",
        {{"label: string"}, {"selected: bool"}, {"onChange: function"}},
        "Radio button."}},
      {"ToggleSwitch",
       {"ToggleSwitch(checked, onChange)",
        {{"checked: bool"}, {"onChange: function"}},
        "Toggle switch."}},
      {"ComboBox",
       {"ComboBox(items, selected, onChange)",
        {{"items: list"}, {"selected: int"}, {"onChange: function"}},
        "Dropdown."}},
      {"ListBox",
       {"ListBox(items, onSelect)",
        {{"items: list"}, {"onSelect: function"}},
        "List box."}},
      {"PasswordBox",
       {"PasswordBox(id, placeholder)",
        {{"id: string"}, {"placeholder: string"}},
        "Password input."}},
      {"Hyperlink",
       {"Hyperlink(text, url)",
        {{"text: string"}, {"url: string"}},
        "Hyperlink."}},
      {"Expander",
       {"Expander(header, content)",
        {{"header: string"}, {"content: any"}},
        "Collapsible panel."}},
      {"DataGrid",
       {"DataGrid(data, columns)",
        {{"data: list"}, {"columns: list"}},
        "Data grid."}},
      {"Canvas",
       {"Canvas(width, height, drawFn)",
        {{"width: number"}, {"height: number"}, {"drawFn: function"}},
        "2D canvas."}},
      {"Tooltip",
       {"Tooltip(text, child)",
        {{"text: string"}, {"child: any"}},
        "Tooltip."}},
      {"Popup", {"Popup(content)", {{"content: any"}}, "Popup."}},
      {"Window",
       {"Window(title, content)",
        {{"title: string"}, {"content: any"}},
        "Sub-window."}},
      {"Animate", {"Animate(props)", {{"props: map"}}, "UI animation."}},
  };
  return sigs;
}

// Figure out which parameter index the cursor is on inside a function call
// by counting commas at the same paren depth.
static int get_active_parameter(const std::string &src, int req_line,
                                int req_char) {
  std::string ln = get_doc_line(src, req_line);
  int limit = std::min(req_char, (int)ln.size());
  int depth = 0, param_idx = 0;
  for (int i = limit - 1; i >= 0; i--) {
    char c = ln[i];
    if (c == ')' || c == ']') {
      depth++;
      continue;
    }
    if (c == '(' || c == '[') {
      if (depth == 0)
        break;
      depth--;
      continue;
    }
    if (c == ',' && depth == 0)
      param_idx++;
  }
  return param_idx;
}

// Get the function name just before the opening ( at the cursor
static std::string get_call_name(const std::string &src, int req_line,
                                 int req_char) {
  std::string ln = get_doc_line(src, req_line);
  int limit = std::min(req_char, (int)ln.size());
  int depth = 0;
  for (int i = limit - 1; i >= 0; i--) {
    char c = ln[i];
    if (c == ')' || c == ']') {
      depth++;
      continue;
    }
    if (c == '(' || c == '[') {
      if (depth == 0) {
        // Walk left to collect the identifier
        int end = i;
        while (end > 0 && ln[end - 1] == ' ')
          end--;
        int start = end;
        while (start > 0 &&
               (isalnum((unsigned char)ln[start - 1]) || ln[start - 1] == '_'))
          start--;
        return ln.substr(start, end - start);
      }
      depth--;
      continue;
    }
  }
  return "";
}

// =============================================================
// SEMANTIC TOKENS
// tokenTypes index: keyword=0, type=1, class=2, function=3,
//   variable=4, string=5, number=6, comment=7, operator=8,
//   parameter=9, enumMember=10
// =============================================================
static const std::vector<std::string> sem_token_types = {
    "keyword", "type",    "class",    "function",  "variable",  "string",
    "number",  "comment", "operator", "parameter", "enumMember"};
static const std::vector<std::string> sem_token_modifiers = {
    "declaration", "readonly", "defaultLibrary", "deprecated"};

struct SemToken {
  int line, col, len, type, mods;
};

static json build_semantic_tokens_data(const std::string &src) {
  // Use the Sapphire Lexer to get accurate token positions
  Lexer lexer(src);
  std::vector<SemToken> tokens;

  // Build a set of known built-in names for quick lookup
  static const std::unordered_set<std::string> builtins = {"clock",
                                                           "parseDouble",
                                                           "valueToString",
                                                           "evaluate",
                                                           "len",
                                                           "createInstance",
                                                           "stringCharAt",
                                                           "stringLength",
                                                           "stringSubstring",
                                                           "stringSplit",
                                                           "stringReplace",
                                                           "stringToUpper",
                                                           "stringToLower",
                                                           "stringTrim",
                                                           "stringContains",
                                                           "getQuote",
                                                           "readLine",
                                                           "printColor",
                                                           "writeFile",
                                                           "readFile",
                                                           "exists",
                                                           "deleteFile",
                                                           "appendFile",
                                                           "openFileDialog",
                                                           "sqrt",
                                                           "rand",
                                                           "abs",
                                                           "floor",
                                                           "ceil",
                                                           "sin",
                                                           "cos",
                                                           "log",
                                                           "pow",
                                                           "min",
                                                           "max",
                                                           "clamp",
                                                           "lerp",
                                                           "lruCreate",
                                                           "lruGet",
                                                           "lruPut",
                                                           "lruHas",
                                                           "listCreate",
                                                           "listAppend",
                                                           "listGet",
                                                           "listSet",
                                                           "listLength",
                                                           "listRemoveAt",
                                                           "listContains",
                                                           "getEnv",
                                                           "getOS",
                                                           "sleep",
                                                           "getClipboard",
                                                           "exec",
                                                           "join",
                                                           "getCoreCount",
                                                           "httpGet",
                                                           "httpPost",
                                                           "httpPing",
                                                           "httpDownload",
                                                           "httpServer",
                                                           "hexToRGB",
                                                           "checkCollision",
                                                           "printStack",
                                                           "dumpGlobals",
                                                           "Render",
                                                           "Style",
                                                           "Flex",
                                                           "Button",
                                                           "Text",
                                                           "Display",
                                                           "Checkbox",
                                                           "Slider",
                                                           "Input",
                                                           "GetInputText",
                                                           "Separator",
                                                           "Menu",
                                                           "MenuItem",
                                                           "Grid",
                                                           "StackPanel",
                                                           "DockPanel",
                                                           "WrapPanel",
                                                           "ScrollView",
                                                           "Border",
                                                           "Image",
                                                           "ProgressBar",
                                                           "RadioBox",
                                                           "ToggleSwitch",
                                                           "ComboBox",
                                                           "ListBox",
                                                           "PasswordBox",
                                                           "Hyperlink",
                                                           "Expander",
                                                           "DataGrid",
                                                           "Canvas",
                                                           "Tooltip",
                                                           "Popup",
                                                           "Window",
                                                           "Animate"};

  for (;;) {
    Token tok = lexer.scan_token();
    if (tok.type == TokenType::TOKEN_END_OF_FILE)
      break;
    if (tok.type == TokenType::TOKEN_ILLEGAL)
      continue;

    SemToken st;
    st.line = tok.line - 1; // LSP 0-based
    st.col = tok.column - 1;
    st.len = tok.length;
    st.mods = 0;

    switch (tok.type) {
    case TokenType::TOKEN_IF:
    case TokenType::TOKEN_ELSE:
    case TokenType::TOKEN_WHILE:
    case TokenType::TOKEN_FOR:
    case TokenType::TOKEN_FOREACH:
    case TokenType::TOKEN_IN:
    case TokenType::TOKEN_OF:
    case TokenType::TOKEN_RETURN:
    case TokenType::TOKEN_BREAK:
    case TokenType::TOKEN_CONTINUE:
    case TokenType::TOKEN_SWITCH:
    case TokenType::TOKEN_CASE:
    case TokenType::TOKEN_DEFAULT:
    case TokenType::TOKEN_TRY:
    case TokenType::TOKEN_CATCH:
    case TokenType::TOKEN_THROW:
    case TokenType::TOKEN_FINALLY:
    case TokenType::TOKEN_IMPORT:
    case TokenType::TOKEN_ASYNC:
    case TokenType::TOKEN_AWAIT:
    case TokenType::TOKEN_SPAWN:
    case TokenType::TOKEN_PRINT:
    case TokenType::TOKEN_AND:
    case TokenType::TOKEN_OR:
    case TokenType::TOKEN_NEW:
      st.type = 0; // keyword
      break;
    case TokenType::TOKEN_FUNCTION:
      st.type = 0;
      st.mods = 1; // keyword | declaration
      break;
    case TokenType::TOKEN_CLASS:
      st.type = 0;
      st.mods = 1;
      break;
    case TokenType::TOKEN_VAR:
    case TokenType::TOKEN_CONST:
      st.type = 0;
      st.mods = (tok.type == TokenType::TOKEN_CONST) ? 3 : 1;
      break;
    case TokenType::TOKEN_THIS:
    case TokenType::TOKEN_SUPER:
      st.type = 0;
      break;
    case TokenType::TOKEN_INT:
    case TokenType::TOKEN_BOOL:
    case TokenType::TOKEN_STRING:
    case TokenType::TOKEN_DOUBLE:
    case TokenType::TOKEN_FLOAT:
    case TokenType::TOKEN_VOID:
      st.type = 1; // type
      break;
    case TokenType::TOKEN_ENUM:
      st.type = 1;
      st.mods = 1;
      break;
    case TokenType::TOKEN_TRUE:
    case TokenType::TOKEN_FALSE:
    case TokenType::TOKEN_NIL:
      st.type = 6; // number/literal â€” use number slot for constants
      break;
    case TokenType::TOKEN_NUMBER:
      st.type = 6; // number
      break;
    case TokenType::TOKEN_STRING_LITERAL:
      st.type = 5; // string
      st.len += 2; // account for the surrounding quotes
      st.col = std::max(0, st.col - 1);
      break;
    case TokenType::TOKEN_IDENTIFIER:
      if (builtins.count(tok.literal)) {
        st.type = 3; // function
        st.mods = 4; // defaultLibrary
      } else {
        st.type = 4; // variable
      }
      break;
    default:
      st.type = 8; // operator / punctuation
      break;
    }

    if (st.line >= 0 && st.col >= 0 && st.len > 0)
      tokens.push_back(st);
  }

  // Encode as delta-encoded flat array
  json data = json::array();
  int prev_line = 0, prev_col = 0;
  for (auto &t : tokens) {
    int delta_line = t.line - prev_line;
    int delta_col = (delta_line == 0) ? (t.col - prev_col) : t.col;
    data.push_back(delta_line);
    data.push_back(delta_col);
    data.push_back(t.len);
    data.push_back(t.type);
    data.push_back(t.mods);
    prev_line = t.line;
    prev_col = t.col;
  }
  return data;
}

// =============================================================
// FOLDING RANGES â€” match { } and detect block structure
// =============================================================
static json build_folding_ranges(const std::string &src) {
  json ranges = json::array();
  std::vector<int> brace_stack;
  std::istringstream ss(src);
  std::string ln;
  int line_no = 0;
  while (std::getline(ss, ln)) {
    for (char c : ln) {
      if (c == '{')
        brace_stack.push_back(line_no);
      else if (c == '}' && !brace_stack.empty()) {
        int start = brace_stack.back();
        brace_stack.pop_back();
        if (line_no > start) {
          ranges.push_back({{"startLine", start},
                            {"endLine", line_no - 1},
                            {"kind", "region"}});
        }
      }
    }
    line_no++;
  }
  return ranges;
}

// =============================================================
// BASIC FORMATTER â€” normalize indentation (4-space, expand tabs)
// =============================================================
static std::string format_document(const std::string &src) {
  std::string result;
  int indent = 0;
  std::istringstream ss(src);
  std::string ln;
  while (std::getline(ss, ln)) {
    // Strip trailing whitespace and \r
    while (!ln.empty() &&
           (ln.back() == ' ' || ln.back() == '\t' || ln.back() == '\r'))
      ln.pop_back();

    // Decrease indent before lines starting with }
    std::string trimmed = ln;
    size_t first = trimmed.find_first_not_of(" \t");
    if (first != std::string::npos)
      trimmed = trimmed.substr(first);

    if (!trimmed.empty() && trimmed[0] == '}')
      indent = std::max(0, indent - 1);

    if (!trimmed.empty()) {
      result += std::string(indent * 4, ' ') + trimmed + "\n";
    } else {
      result += "\n";
    }

    // Increase indent after lines ending with {
    if (!trimmed.empty() && trimmed.back() == '{')
      indent++;
  }
  return result;
}

// =============================================================
// MAIN LSP LOOP
// =============================================================
void run_lsp() {
  // Disable IO sync to avoid polluting stdout
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(NULL);

  // Build completion list and signatures once at startup
  static const json all_completion_items = build_all_completion_items();
  const auto &sigs = get_signatures();

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty() || line == "\r")
      continue;

    if (line.rfind("Content-Length:", 0) == 0) {
      try {
        int content_length = std::stoi(line.substr(15));
        std::getline(std::cin, line); // consume blank separator

        std::string body(content_length, '\0');
        std::cin.read(&body[0], content_length);

        json request = json::parse(body);

        if (request.contains("method")) {
          std::cerr << "[Sapphire LSP] Received method: " << request["method"]
                    << std::endl;
        }

        if (request.contains("method")) {
          std::string method = request["method"];

          // =========================================================
          if (method == "initialize") {
            json response = {
                {"jsonrpc", "2.0"},
                {"id", request["id"]},
                {"result",
                 {{"capabilities",
                   {// Full document sync
                    {"textDocumentSync",
                     {{"openClose", true},
                      {"change", 1},
                      {"save", {{"includeText", true}}}}},
                    // Completion
                    {"completionProvider",
                     {{"resolveProvider", false},
                      {"triggerCharacters", {".", " ", "("}}}},
                    // Hover
                    {"hoverProvider", true},
                    // Signature help
                    {"signatureHelpProvider",
                     {{"triggerCharacters", {"(", ","}},
                      {"retriggerCharacters", {","}}}},
                    // Definition
                    {"definitionProvider", true},
                    // References
                    {"referencesProvider", true},
                    // Document highlight
                    {"documentHighlightProvider", true},
                    // Document symbols (outline)
                    {"documentSymbolProvider", true},
                    // Rename
                    {"renameProvider", {{"prepareProvider", false}}},
                    // Formatting
                    {"documentFormattingProvider", true},
                    // Folding ranges
                    {"foldingRangeProvider", true},
                    // Semantic tokens
                    {"semanticTokensProvider",
                     {{"legend",
                       {{"tokenTypes", sem_token_types},
                        {"tokenModifiers", sem_token_modifiers}}},
                      {"full", true},
                      {"range", false}}},
                    // Diagnostics (push-based)
                    {"diagnosticProvider",
                     {{"interFileDependencies", false},
                      {"workspaceDiagnostics", false}}}}},
                  {"serverInfo",
                   {{"name", "Sapphire Language Server"},
                    {"version", "1.0.9"}}}}}};
            send_lsp_message(response);
            std::cerr << "[Sapphire LSP] Handshake complete. Server connected."
                      << std::endl;
          }
          // =========================================================
          else if (method == "textDocument/didOpen") {
            auto &params = request["params"];
            if (params.contains("textDocument")) {
              auto &td = params["textDocument"];
              if (td.contains("text"))
                current_document_content = td["text"];
              if (td.contains("uri"))
                current_document_uri = td["uri"];
            }
            std::cerr << "[Sapphire LSP] Document opened: "
                      << current_document_content.length() << " chars"
                      << std::endl;
            publish_diagnostics(current_document_uri, current_document_content);
          }
          // =========================================================
          else if (method == "textDocument/didChange") {
            auto &params = request["params"];
            if (params.contains("textDocument") &&
                params["textDocument"].contains("uri"))
              current_document_uri = params["textDocument"]["uri"];
            if (params.contains("contentChanges") &&
                !params["contentChanges"].empty())
              current_document_content = params["contentChanges"][0]["text"];
            std::cerr << "[Sapphire LSP] Document updated: "
                      << current_document_content.length() << " chars"
                      << std::endl;
            publish_diagnostics(current_document_uri, current_document_content);
          }
          // =========================================================
          else if (method == "textDocument/didSave") {
            auto &params = request["params"];
            if (params.contains("textDocument") &&
                params["textDocument"].contains("uri"))
              current_document_uri = params["textDocument"]["uri"];
            if (params.contains("text"))
              current_document_content = params["text"];
            publish_diagnostics(current_document_uri, current_document_content);
          }
          // =========================================================
          else if (method == "textDocument/completion") {
            std::string prefix;
            try {
              auto &params = request["params"];
              int req_line = params["position"]["line"];
              int req_char = params["position"]["character"];
              std::string cur =
                  get_doc_line(current_document_content, req_line);
              int limit = std::min(req_char, (int)cur.size());
              int i = limit;
              while (i > 0 &&
                     (isalnum((unsigned char)cur[i - 1]) || cur[i - 1] == '_'))
                i--;
              prefix = cur.substr(i, limit - i);
            } catch (...) {
            }

            json filtered = json::array();
            for (const auto &item : all_completion_items) {
              std::string label = item["label"];
              if (prefix.empty()) {
                filtered.push_back(item);
              } else if (label.size() >= prefix.size()) {
                std::string lp = label.substr(0, prefix.size());
                std::string pp = prefix;
                std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
                std::transform(pp.begin(), pp.end(), pp.begin(), ::tolower);
                if (lp == pp)
                  filtered.push_back(item);
              }
            }
            send_lsp_message(
                {{"jsonrpc", "2.0"},
                 {"id", request["id"]},
                 {"result", {{"isIncomplete", false}, {"items", filtered}}}});
            std::cerr << "[Sapphire LSP] Sent " << filtered.size()
                      << " completion(s) (prefix: '" << prefix << "')"
                      << std::endl;
          }
          // =========================================================
          else if (method == "textDocument/hover") {
            std::string word;
            try {
              auto &params = request["params"];
              word = get_word_at(current_document_content,
                                 params["position"]["line"],
                                 params["position"]["character"]);
            } catch (...) {
            }

            auto it = hover_docs.find(word);
            if (!word.empty() && it != hover_docs.end()) {
              send_lsp_message(
                  {{"jsonrpc", "2.0"},
                   {"id", request["id"]},
                   {"result",
                    {{"contents",
                      {{"kind", "markdown"}, {"value", it->second}}}}}});
            } else {
              send_lsp_message({{"jsonrpc", "2.0"},
                                {"id", request["id"]},
                                {"result", nullptr}});
            }
          }
          // =========================================================
          else if (method == "textDocument/signatureHelp") {
            std::string func_name;
            int active_param = 0;
            try {
              auto &params = request["params"];
              int req_line = params["position"]["line"];
              int req_char = params["position"]["character"];
              func_name =
                  get_call_name(current_document_content, req_line, req_char);
              active_param = get_active_parameter(current_document_content,
                                                  req_line, req_char);
            } catch (...) {
            }

            auto sit = sigs.find(func_name);
            if (sit != sigs.end()) {
              const auto &sig = sit->second;
              json params_arr = json::array();
              for (auto &p : sig.params) {
                params_arr.push_back({{"label", p.label}});
              }
              send_lsp_message(
                  {{"jsonrpc", "2.0"},
                   {"id", request["id"]},
                   {"result",
                    {{"signatures",
                      json::array(
                          {json({{"label", sig.label},
                                 {"documentation", sig.doc},
                                 {"parameters", params_arr},
                                 {"activeParameter",
                                  std::min(active_param,
                                           (int)sig.params.size() - 1)}})})},
                     {"activeSignature", 0},
                     {"activeParameter", active_param}}}});
            } else {
              send_lsp_message({{"jsonrpc", "2.0"},
                                {"id", request["id"]},
                                {"result", nullptr}});
            }
          }
          // =========================================================
          else if (method == "textDocument/documentSymbol") {
            auto symbols = scan_document_symbols(current_document_content);
            json sym_arr = json::array();
            for (auto &s : symbols) {
              sym_arr.push_back(
                  {{"name", s.name},
                   {"kind", s.kind},
                   {"range",
                    {{"start", {{"line", s.line}, {"character", s.col}}},
                     {"end",
                      {{"line", s.end_line}, {"character", s.end_col}}}}},
                   {"selectionRange",
                    {{"start", {{"line", s.line}, {"character", s.col}}},
                     {"end",
                      {{"line", s.line},
                       {"character", s.col + (int)s.name.size()}}}}}});
            }
            send_lsp_message({{"jsonrpc", "2.0"},
                              {"id", request["id"]},
                              {"result", sym_arr}});
            std::cerr << "[Sapphire LSP] Document symbols: " << sym_arr.size()
                      << std::endl;
          }
          // =========================================================
          else if (method == "textDocument/documentHighlight") {
            std::string word;
            try {
              auto &params = request["params"];
              word = get_word_at(current_document_content,
                                 params["position"]["line"],
                                 params["position"]["character"]);
            } catch (...) {
            }

            json highlights = json::array();
            if (!word.empty()) {
              std::istringstream ss(current_document_content);
              std::string ln;
              int line_no = 0;
              while (std::getline(ss, ln)) {
                if (!ln.empty() && ln.back() == '\r')
                  ln.pop_back();
                size_t pos = 0;
                while ((pos = ln.find(word, pos)) != std::string::npos) {
                  // Ensure it's a whole-word match
                  bool left_ok =
                      (pos == 0 || !isalnum((unsigned char)ln[pos - 1]) &&
                                       ln[pos - 1] != '_');
                  bool right_ok =
                      (pos + word.size() >= ln.size() ||
                       !isalnum((unsigned char)ln[pos + word.size()]) &&
                           ln[pos + word.size()] != '_');
                  if (left_ok && right_ok) {
                    highlights.push_back({
                        {"range",
                         {{"start",
                           {{"line", line_no}, {"character", (int)pos}}},
                          {"end",
                           {{"line", line_no},
                            {"character", (int)(pos + word.size())}}}}},
                        {"kind", 1} // 1=Text, 2=Read, 3=Write
                    });
                  }
                  pos += word.size();
                }
                line_no++;
              }
            }
            send_lsp_message({{"jsonrpc", "2.0"},
                              {"id", request["id"]},
                              {"result", highlights}});
          }
          // =========================================================
          else if (method == "textDocument/definition") {
            std::string word;
            try {
              auto &params = request["params"];
              word = get_word_at(current_document_content,
                                 params["position"]["line"],
                                 params["position"]["character"]);
            } catch (...) {
            }

            json locations = json::array();
            if (!word.empty()) {
              // Search for the first declaration of this identifier
              std::regex re_decl("\\b(?:function|var|const|class|enum)\\s+(" +
                                 word + ")\\b");
              std::istringstream ss(current_document_content);
              std::string ln;
              int line_no = 0;
              while (std::getline(ss, ln)) {
                if (!ln.empty() && ln.back() == '\r')
                  ln.pop_back();
                std::smatch m;
                if (std::regex_search(ln, m, re_decl)) {
                  int col = (int)m.position(1);
                  locations.push_back(
                      {{"uri", current_document_uri},
                       {"range",
                        {{"start", {{"line", line_no}, {"character", col}}},
                         {"end",
                          {{"line", line_no},
                           {"character", col + (int)word.size()}}}}}});
                  break; // first definition only
                }
                line_no++;
              }
            }
            send_lsp_message(
                {{"jsonrpc", "2.0"},
                 {"id", request["id"]},
                 {"result", locations.empty() ? json(nullptr) : locations[0]}});
          }
          // =========================================================
          else if (method == "textDocument/references") {
            std::string word;
            try {
              auto &params = request["params"];
              word = get_word_at(current_document_content,
                                 params["position"]["line"],
                                 params["position"]["character"]);
            } catch (...) {
            }

            json refs = json::array();
            if (!word.empty()) {
              std::istringstream ss(current_document_content);
              std::string ln;
              int line_no = 0;
              while (std::getline(ss, ln)) {
                if (!ln.empty() && ln.back() == '\r')
                  ln.pop_back();
                size_t pos = 0;
                while ((pos = ln.find(word, pos)) != std::string::npos) {
                  bool left_ok =
                      (pos == 0 || !isalnum((unsigned char)ln[pos - 1]) &&
                                       ln[pos - 1] != '_');
                  bool right_ok =
                      (pos + word.size() >= ln.size() ||
                       !isalnum((unsigned char)ln[pos + word.size()]) &&
                           ln[pos + word.size()] != '_');
                  if (left_ok && right_ok) {
                    refs.push_back(
                        {{"uri", current_document_uri},
                         {"range",
                          {{"start",
                            {{"line", line_no}, {"character", (int)pos}}},
                           {"end",
                            {{"line", line_no},
                             {"character", (int)(pos + word.size())}}}}}});
                  }
                  pos += word.size();
                }
                line_no++;
              }
            }
            send_lsp_message(
                {{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", refs}});
          }
          // =========================================================
          else if (method == "textDocument/rename") {
            std::string old_word, new_name;
            try {
              auto &params = request["params"];
              old_word = get_word_at(current_document_content,
                                     params["position"]["line"],
                                     params["position"]["character"]);
              new_name = params["newName"];
            } catch (...) {
            }

            json edits = json::array();
            if (!old_word.empty() && !new_name.empty()) {
              std::istringstream ss(current_document_content);
              std::string ln;
              int line_no = 0;
              while (std::getline(ss, ln)) {
                if (!ln.empty() && ln.back() == '\r')
                  ln.pop_back();
                size_t pos = 0;
                while ((pos = ln.find(old_word, pos)) != std::string::npos) {
                  bool left_ok =
                      (pos == 0 || !isalnum((unsigned char)ln[pos - 1]) &&
                                       ln[pos - 1] != '_');
                  bool right_ok =
                      (pos + old_word.size() >= ln.size() ||
                       !isalnum((unsigned char)ln[pos + old_word.size()]) &&
                           ln[pos + old_word.size()] != '_');
                  if (left_ok && right_ok) {
                    edits.push_back(
                        {{"range",
                          {{"start",
                            {{"line", line_no}, {"character", (int)pos}}},
                           {"end",
                            {{"line", line_no},
                             {"character", (int)(pos + old_word.size())}}}}},
                         {"newText", new_name}});
                  }
                  pos += old_word.size();
                }
                line_no++;
              }
            }
            send_lsp_message(
                {{"jsonrpc", "2.0"},
                 {"id", request["id"]},
                 {"result", {{"changes", {{current_document_uri, edits}}}}}});
            std::cerr << "[Sapphire LSP] Rename '" << old_word << "' -> '"
                      << new_name << "': " << edits.size() << " edits"
                      << std::endl;
          }
          // =========================================================
          else if (method == "textDocument/formatting") {
            std::string formatted = format_document(current_document_content);
            // Count lines in document
            int num_lines = 0;
            for (char c : current_document_content)
              if (c == '\n')
                num_lines++;

            send_lsp_message(
                {{"jsonrpc", "2.0"},
                 {"id", request["id"]},
                 {"result",
                  json::array({json(
                      {{"range",
                        {{"start", {{"line", 0}, {"character", 0}}},
                         {"end", {{"line", num_lines}, {"character", 9999}}}}},
                       {"newText", formatted}})})}});
            std::cerr << "[Sapphire LSP] Formatted document." << std::endl;
          }
          // =========================================================
          else if (method == "textDocument/foldingRange") {
            send_lsp_message(
                {{"jsonrpc", "2.0"},
                 {"id", request["id"]},
                 {"result", build_folding_ranges(current_document_content)}});
          }
          // =========================================================
          else if (method == "textDocument/semanticTokens/full") {
            json data;
            try {
              data = build_semantic_tokens_data(current_document_content);
            } catch (...) {
              data = json::array();
            }
            send_lsp_message({{"jsonrpc", "2.0"},
                              {"id", request["id"]},
                              {"result", {{"data", data}}}});
            std::cerr << "[Sapphire LSP] Semantic tokens: " << (data.size() / 5)
                      << " token(s)" << std::endl;
          }
          // =========================================================
          else if (method == "shutdown") {
            send_lsp_message({{"jsonrpc", "2.0"},
                              {"id", request["id"]},
                              {"result", nullptr}});
          } else if (method == "exit") {
            break;
          }
          // Notifications with no id don't need a response

        } else if (request.contains("id")) {
          // Respond null to unsupported requests to prevent editor hangs
          send_lsp_message(
              {{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", nullptr}});
        }

      } catch (const std::exception &e) {
        std::cerr << "[Sapphire LSP] Error: " << e.what() << std::endl;
      }
    }
  }
}

// ---------------------------------------------------------
// COMPILADOR / CLI NORMAL
// ---------------------------------------------------------
// ---------------------------------------------------------

int run_compiler(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: sapphire compile <input.sp> <output.sbc>" << std::endl;
    return 1;
  }

  std::string input_path = argv[1];
  std::string output_path = argv[2];

  std::string source = load_file_as_string(input_path);
  if (source.empty()) {
    std::cerr << "Error: Could not read input file: " << input_path
              << std::endl;
    return 1;
  }

  bool soft_mode_enabled = check_for_soft_mode(source);

  VM vm;
  vm.soft_mode = soft_mode_enabled;

  Preprocessor prep;
  std::string processed_source = prep.process(source);
  ObjFunction *main_function = compile(&vm, processed_source);

  if (main_function == nullptr) {
    std::cerr << "Compilation error in input file." << std::endl;
    return 1;
  }

  serialize_function(main_function, &vm, output_path);
  std::cout << "Compilation completed: " << output_path << std::endl;

  return 0;
}

void display_info() {
  std::vector<std::string> info_lines = {
      "",
      ">>> Welcome to Sapphire <<<",
      "",
      "** SAPPHIRE INFORMATIONS: **",
      "",
      "Version: 1.0.9 (build 0223-07182026 (July 18, 2026))",
      "Release Date: July 18, 2026",
      "",
      "Developed by: Bernardo Alvim",
      "Protected by MIT License",
      "",
      "--- Usage Options ---",
      "sapphire                         : Starts the interactive REPL",
      "sapphire <script_path.sp>        : Executes a source script",
      "sapphire run <script_path.sp>    : Executes a source script",
      "sapphire <bytecode_path.sbc>     : Executes a bytecode file",
      "sapphire -e \"<code>\"             : Executes inline code",
      "sapphire check <script_path.sp>  : Checks syntax without running",
      "sapphire disasm <script_path.sp> : Prints VM bytecode disassembly",
      "sapphire init <project_name>     : Initializes a new project skeleton",
      "sapphire lsp                     : Starts the Language Server Protocol",
      "sapphire clean                   : Removes all .sbc generated files",
      "sapphire compile <in> <out>      : Compiles a script to bytecode",
      "sapphire -v | --version          : Displays version information",
      "sapphire -h | --help | --info    : Displays this information screen",
      "sapphire test <script_path.sp>   : Runs tests in a script",
      "sapphire lint <script_path.sp>   : Lints a script",
      "",
      "--- Tool Options ---",
      "beryl <command>                  : Executes the Beryl tool (for packing "
      "files into .exe executables)",
      "topaz <command>                  : Executes the Topaz help command "
      "(plugin and version manager)",
      "citrine <command>                : Executes the Citrine static analysis linter",
      "garnet <command>                 : Executes the Garnet parallel test runner",
      "amethyst <command>               : Executes the Amethyst automatic code formatter",
      "",
      "Thank you!",
      "",
      "GitHub: github.com/foxzyt/sapphire"};

  for (const auto &line : info_lines) {
    std::cout << line << std::endl;
  }
}

void display_version() {
  std::cout << "Sapphire 1.0.9 (build 0223-07182026)" << std::endl;
}

std::string load_source_script(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return "";
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void load_config_from_script(const std::string &path, ScriptConfig &config) {
  std::ifstream file(path);
  if (!file.is_open())
    return;
  std::string line;
  while (std::getline(file, line)) {
    size_t comment_pos = line.find("//");
    if (comment_pos != std::string::npos)
      line = line.substr(0, comment_pos);
    auto delimiterPos = line.find("=");
    if (delimiterPos != std::string::npos) {
      std::string key = line.substr(0, delimiterPos);
      std::string value = line.substr(delimiterPos + 1);
      key.erase(std::remove_if(key.begin(), key.end(), isspace), key.end());
      value.erase(std::remove_if(value.begin(), value.end(), isspace),
                  value.end());
      if (key.find("config_window_width") != std::string::npos) {
        try {
          config.windowWidth = std::stoi(value);
        } catch (...) {
        }
      } else if (key.find("config_window_height") != std::string::npos) {
        try {
          config.windowHeight = std::stoi(value);
        } catch (...) {
        }
      } else if (key.find("config_window_borderless") != std::string::npos) {
        if (value == "true" || value == "1")
          config.windowBorderless = true;
        else
          config.windowBorderless = false;
      } else if (key.find("config_window_title") != std::string::npos) {
        auto start_quote = line.find("\"");
        auto end_quote = line.rfind("\"");
        if (start_quote != std::string::npos && end_quote != start_quote) {
          config.windowTitle =
              line.substr(start_quote + 1, end_quote - start_quote - 1);
        }
      }
    }
  }
}

bool script_uses_ui(const std::string &source) {
  Lexer lexer(source);
  for (;;) {
    Token token = lexer.scan_token();
    if (token.type == TokenType::TOKEN_IDENTIFIER && token.literal == "UI")
      return true;
    if (token.type == TokenType::TOKEN_END_OF_FILE)
      break;
  }
  return false;
}

void run_ui_mode(std::string script_content, const ScriptConfig &config,
                 const std::string &script_path) {
  uint32_t style =
      config.windowBorderless ? sf::Style::None : sf::Style::Default;
  sf::RenderWindow window(
      sf::VideoMode({config.windowWidth, config.windowHeight}),
      config.windowTitle, style);
  window.setFramerateLimit(60);

  bool soft_mode_enabled = check_for_soft_mode(script_content);
  VM vm(config, true, &window);
  g_current_vm = &vm;
  vm.soft_mode = soft_mode_enabled;
  vm.add_module_search_path(
      std::filesystem::path(script_path).parent_path().string());

  Preprocessor prep;
  script_content = prep.process(script_content);
  ObjFunction *main_script_func = compile(&vm, script_content);
  if (main_script_func == nullptr) {
    window.close();
    return;
  }

  vm.call_and_run(main_script_func);

  SapphireValue update_fn_val = vm.getGlobal("updateUI");
  if (update_fn_val.type != ValType::VAL_OBJ ||
      update_fn_val.as.obj->type != OBJ_CLOSURE) {
    std::cerr << "Error: updateUI() function not found in script." << std::endl;
    window.close();
    return;
  }

  ObjClosure *update_closure =
      static_cast<ObjClosure *>(update_fn_val.as.obj);
  ObjFunction *update_function = update_closure->function;

  auto last_write = std::filesystem::last_write_time(script_path);

  while (window.isOpen()) {
    try {
      auto current_write = std::filesystem::last_write_time(script_path);
      if (current_write > last_write) {
        last_write = current_write;
        std::cout << "Reloading script: " << script_path << std::endl;
        std::string new_content = load_source_script(script_path);
        new_content = prep.process(new_content);
        ObjFunction *new_func = compile(&vm, new_content);
        if (new_func) {
          vm.call_and_run(new_func);
          SapphireValue new_update_fn_val = vm.getGlobal("updateUI");
          if (new_update_fn_val.type == ValType::VAL_OBJ &&
              new_update_fn_val.as.obj->type == OBJ_CLOSURE) {
            update_closure = static_cast<ObjClosure *>(
                new_update_fn_val.as.obj);
            update_function = update_closure->function;
          }
        }
      }
    } catch (const std::filesystem::filesystem_error &) {
    }

    vm.resetStack();

    if (!vm.call_and_run(update_function)) {
      break;
    }
  }
  g_current_vm = nullptr;
}

void run_file_mode(const std::string &script_content,
                   const std::string &script_path) {
  VM vm;
  g_current_vm = &vm;
  vm.soft_mode = check_for_soft_mode(script_content);
  vm.add_module_search_path(
      std::filesystem::path(script_path).parent_path().string());
  Preprocessor prep;
  std::string processed = prep.process(script_content);
  (void)vm.interpret(processed);
  g_current_vm = nullptr;
}

void run_repl() {
  std::cout << "Sapphire REPL v1.0.9\nType 'exit' or 'quit' to close.\n";
  VM vm;
  g_current_vm = &vm;
  std::string line;
  while (true) {
    std::cout << ">> ";
    if (!std::getline(std::cin, line))
      break;
    if (line == "exit" || line == "quit")
      break;
    if (line.empty())
      continue;

    ObjFunction *func = compile(&vm, line);
    if (func) {
      vm.call_and_run(func);
      vm.resetStack();
    }
  }
  g_current_vm = nullptr;
}

void run_eval(const std::string &code) {
  VM vm;
  g_current_vm = &vm;
  ObjFunction *func = compile(&vm, code);
  if (func) {
    vm.call_and_run(func);
  }
  g_current_vm = nullptr;
}

void run_check(const std::string &path) {
  std::string source = load_source_script(path);
  if (source.empty()) {
    std::cerr << "Error: Could not read " << path << std::endl;
    return;
  }
  VM vm;
  Preprocessor prep;
  std::string processed = prep.process(source);
  ObjFunction *func = compile(&vm, processed);
  if (func) {
    std::cout << "Syntax OK: " << path << std::endl;
  } else {
    std::cerr << "Syntax Error: " << path << std::endl;
  }
}

void run_test(const std::string &path) {
  std::vector<std::string> test_files;

  try {
    if (std::filesystem::is_directory(path)) {
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
          std::string fname = entry.path().filename().string();
          if ((fname.rfind("test_", 0) == 0 ||
               fname.find("_test.sp") != std::string::npos ||
               fname.find("test.sp") != std::string::npos) &&
              entry.path().extension() == ".sp") {
            test_files.push_back(entry.path().string());
          }
        }
      }
    } else {
      test_files.push_back(path);
    }
  } catch (...) {
    std::cerr << "Error: Directory access failed " << path << std::endl;
    return;
  }

  if (test_files.empty()) {
    std::cout << tc_yellow() << "No test files found matching: " << path
              << tc_reset() << std::endl;
    return;
  }

  int total_tests = 0;
  int passed_tests = 0;
  int failed_tests = 0;

  std::cout << tc_bold() << tc_cyan()
            << "========================================" << tc_reset()
            << std::endl;
  std::cout << tc_bold() << tc_cyan() << "       SAPPHIRE TEST RUNNER     "
            << tc_reset() << std::endl;
  std::cout << tc_bold() << tc_cyan()
            << "========================================" << tc_reset()
            << std::endl;

  for (const auto &file : test_files) {
    std::cout << tc_bold() << "\nRunning tests in: " << file << tc_reset()
              << std::endl;

    std::string source = load_source_script(file);
    if (source.empty()) {
      std::cerr << tc_red() << "  [!] Error: Could not read " << file
                << tc_reset() << std::endl;
      failed_tests++;
      continue;
    }

    VM vm;
    g_current_vm = &vm;
    vm.add_module_search_path(
        std::filesystem::path(file).parent_path().string());

    Preprocessor prep;
    std::string processed = prep.process(source);

    try {
      vm.interpret(processed);
    } catch (const std::exception &e) {
      std::cerr << tc_red() << "  [!] File execution failure: " << e.what()
                << tc_reset() << std::endl;
      failed_tests++;
      g_current_vm = nullptr;
      continue;
    }

    std::vector<std::string> global_tests;
    std::vector<std::pair<std::string, std::string>> class_tests;

    for (const auto &pair : vm.globals) {
      std::string name = pair.first;
      SapphireValue val = pair.second;

      if (name.rfind("test", 0) == 0 || name.rfind("should", 0) == 0) {
        if (is_obj_type(val, OBJ_FUNCTION) || is_obj_type(val, OBJ_CLOSURE) ||
            is_obj_type(val, OBJ_NATIVE)) {
          global_tests.push_back(name);
        }
      }

      if (name.rfind("Test", 0) == 0 || name.rfind("test", 0) == 0) {
        if (is_obj_type(val, OBJ_CLASS)) {
          ObjClass *klass =
              static_cast<ObjClass *>(val.as.obj);
          for (const auto &method_pair : klass->methods) {
            std::string method_name = method_pair.first;
            if (method_name.rfind("test", 0) == 0 ||
                method_name.rfind("should", 0) == 0) {
              class_tests.push_back({name, method_name});
            }
          }
        }
      }
    }

    std::sort(global_tests.begin(), global_tests.end());
    std::sort(class_tests.begin(), class_tests.end());

    for (const auto &test_name : global_tests) {
      total_tests++;
      std::cout << "  - " << test_name << " ... ";

      try {
        std::string test_code = test_name + "();";
        vm.interpret(test_code);
        std::cout << tc_green() << "PASSED" << tc_reset() << std::endl;
        passed_tests++;
      } catch (const std::exception &e) {
        std::cout << tc_red() << "FAILED (" << e.what() << ")" << tc_reset()
                  << std::endl;
        failed_tests++;
      }
    }

    for (const auto &class_test : class_tests) {
      total_tests++;
      std::string class_name = class_test.first;
      std::string method_name = class_test.second;
      std::cout << "  - " << class_name << "." << method_name << " ... ";

      try {
        std::string test_code = "var test_inst = " + class_name +
                                "(); test_inst." + method_name + "();";
        vm.interpret(test_code);
        std::cout << tc_green() << "PASSED" << tc_reset() << std::endl;
        passed_tests++;
      } catch (const std::exception &e) {
        std::cout << tc_red() << "FAILED (" << e.what() << ")" << tc_reset()
                  << std::endl;
        failed_tests++;
      }
    }

    g_current_vm = nullptr;
  }

  std::cout << tc_bold() << tc_cyan()
            << "\n========================================" << tc_reset()
            << std::endl;
  std::cout << "Test Summary:" << std::endl;
  std::cout << "  Total:  " << total_tests << std::endl;
  std::cout << "  Passed: " << tc_green() << passed_tests << tc_reset()
            << std::endl;
  std::cout << "  Failed: " << (failed_tests > 0 ? tc_red() : tc_green())
            << failed_tests << tc_reset() << std::endl;
  std::cout << tc_bold() << tc_cyan()
            << "========================================" << tc_reset()
            << std::endl;
}

void run_disasm(const std::string &path) {
  std::string source = load_source_script(path);
  if (source.empty()) {
    std::cerr << "Error: Could not read " << path << std::endl;
    return;
  }
  VM vm;
  Preprocessor prep;
  std::string processed = prep.process(source);
  ObjFunction *func = compile(&vm, processed);
  if (func) {
    disassemble_chunk(func->chunk, path);
  } else {
    std::cerr << "Compilation failed." << std::endl;
  }
}

void run_clean() {
  int count = 0;
  try {
    for (const auto &entry : std::filesystem::directory_iterator(".")) {
      if (entry.path().extension() == ".sbc") {
        std::filesystem::remove(entry.path());
        count++;
      }
    }
    std::cout << "Cleaned " << count << " bytecode files." << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error during clean: " << e.what() << std::endl;
  }
}

void run_init(const std::string &name) {
  if (std::filesystem::exists(name)) {
    std::cerr << "Error: Directory '" << name << "' already exists."
              << std::endl;
    return;
  }
  try {
    std::filesystem::create_directory(name);
    std::filesystem::create_directory(name + "/build");

    std::ofstream info(name + "/ProjectInfo.txt");
    info << "Project=" << name << "\n"
         << "Author=Sapphire Developer\n"
         << "Version=1.0.0\n"
         << "Build=1\n"
         << "OutputFile=build/app.exe\n"
         << "Date=" << std::time(nullptr) << "\n";
    info.close();

    std::ofstream theme(name + "/theme.sp");
    theme << "// Theme definitions\n"
          << "var window_width = 800;\n"
          << "var window_height = 600;\n"
          << "var window_title = \"" << name << "\";\n"
          << "var primary_color = \"blue\";\n";
    theme.close();

    std::ofstream main(name + "/main.sp");
    main << "import \"theme.sp\";\n\n"
         << "var config_window_width = window_width;\n"
         << "var config_window_height = window_height;\n"
         << "var config_window_title = window_title;\n"
         << "var UI = 1;\n\n"
         << "function updateUI() {\n"
         << "    pollEvents();\n"
         << "    if (isWindowOpen() == false) { return false; }\n"
         << "    clear();\n"
         << "    display();\n"
         << "    return true;\n"
         << "}\n";
    main.close();

    std::cout << "Initialized Sapphire project in '" << name << "'."
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error initializing project: " << e.what() << std::endl;
  }
}

void run_bytecode_mode(const std::string &bytecode_path,
                       const ScriptConfig &config, bool is_ui_mode) {
  sf::RenderWindow *window_ptr = nullptr;
  std::optional<sf::RenderWindow> temp_window;

  if (is_ui_mode) {
    uint32_t style =
        config.windowBorderless ? sf::Style::None : sf::Style::Default;
    temp_window.emplace(
        sf::VideoMode({config.windowWidth, config.windowHeight}),
        config.windowTitle, style);
    temp_window->setFramerateLimit(60);
    window_ptr = &(*temp_window);
  }

  VM vm(config, is_ui_mode, window_ptr);
  vm.add_module_search_path(
      std::filesystem::path(bytecode_path).parent_path().string());
  g_current_vm = &vm;

  ObjFunction *main_bytecode_function =
      deserialize_function(&vm, bytecode_path);
  if (main_bytecode_function == nullptr) {
    if (temp_window)
      temp_window->close();
    g_current_vm = nullptr;
    return;
  }

  vm.run_function(main_bytecode_function);

  if (is_ui_mode && temp_window) {
    SapphireValue update_fn_val = vm.getGlobal("updateUI");
    if (update_fn_val.type == ValType::VAL_OBJ &&
        update_fn_val.as.obj->type == OBJ_CLOSURE) {
      ObjFunction *update_function =
          static_cast<ObjClosure *>(update_fn_val.as.obj)
              ->function;

      while (temp_window->isOpen()) {
        while (const std::optional event = temp_window->pollEvent()) {
          if (event->is<sf::Event::Closed>())
            temp_window->close();
        }
        if (!temp_window->isOpen())
          break;

        vm.ui_state.nextPosX = 10.0f;
        vm.ui_state.nextPosY = 10.0f;

        temp_window->clear(sf::Color(25, 25, 30));
        if (!vm.call_and_run(update_function)) {
          temp_window->close();
          break;
        }
        temp_window->display();
      }
    }
  }
  g_current_vm = nullptr;
}

int main(int argc, char *argv[]) {
  init_terminal();

  if (argc == 1) {
    run_repl();
    return 0;
  }

  std::string command = argv[1];

  if (command == "-h" || command == "--help" || command == "--info") {
    display_info();
  } else if (command == "-v" || command == "--version") {
    display_version();
  } else if (command == "lsp") {
    run_lsp();
    return 0;
  } else if (command == "compile" && argc == 4) {
    return run_compiler(argc - 1, &argv[1]);
  } else if (command == "eval" || command == "-e") {
    if (argc >= 3)
      run_eval(argv[2]);
    else
      std::cerr << "Usage: sapphire eval \"<code>\"" << std::endl;
  } else if (command == "check") {
    if (argc >= 3)
      run_check(argv[2]);
    else
      std::cerr << "Usage: sapphire check <file>" << std::endl;
  } else if (command == "disasm") {
    if (argc >= 3)
      run_disasm(argv[2]);
    else
      std::cerr << "Usage: sapphire disasm <file>" << std::endl;
  } else if (command == "clean") {
    run_clean();
  } else if (command == "init") {
    if (argc >= 3)
      run_init(argv[2]);
    else
      std::cerr << "Usage: sapphire init <project_name>" << std::endl;
  } else if (command == "test") {
    if (argc >= 3)
      run_test(argv[2]);
    else
      run_test(".");

  } else {
    std::string path = (command == "run" && argc >= 3) ? argv[2] : command;
    std::string ext = std::filesystem::path(path).extension().string();
    ScriptConfig config;
    load_config_from_script(path, config);

    if (ext == ".sbc") {
      bool is_ui = (path.find("ui") != std::string::npos);
      run_bytecode_mode(path, config, is_ui);
    } else if (ext == ".sp") {
      std::string content = load_source_script(path);
      if (content.empty())
        return 1;
      if (script_uses_ui(content))
        run_ui_mode(content, config, path);
      else
        run_file_mode(content, path);
    } else {
      std::cerr << "Error: Unknown command or invalid file extension: "
                << command << std::endl;
      return 1;
    }
  }

  return 0;
}







