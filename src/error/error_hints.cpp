#include "error_hints.h"
#include <string>

void inject_syntax_hints(const std::string& message, std::shared_ptr<SapphireError>& error) {
    if (message == "Invalid assignment target.") {
        error->add_suggestion("We can't assign a value here. You're trying to assign a value to something that isn't a variable (like assigning directly to a number, string, or a result of an expression).", "");
        error->add_suggestion("[Hint] Check if the variable you want to receive the value is on the LEFT side of the '='.", "");
    } else if (message == "Expect ')' after expression.") {
        error->add_suggestion("It looks like you opened a parenthesis '(' but forgot to close it with a ')'.", "");
        error->add_suggestion("[Hint] Double-check your parentheses pairing in this expression. Sometimes it's easy to miss one at the very end of a complex math or logic statement!", "");
    } else if (message == "Expect ';' after expression.") {
        error->add_suggestion("The compiler got a bit confused because this statement didn't end properly.", "");
        error->add_suggestion("[Hint] Did you forget to add a semicolon ';' at the end of the line? Semicolons are required to tell the compiler where one instruction ends and the next begins.", "");
    } else if (message.find("Unexpected character") != std::string::npos) {
        error->add_suggestion("I found a character that doesn't belong in the language syntax.", "");
        error->add_suggestion("[Hint] This usually happens if you accidentally type a special symbol (like a stray backtick or unicode character) that isn't part of a string.", "");
    } else if (message == "Cannot have more than 255 elements in a map literal.") {
        error->add_suggestion("Whoa, that's a huge map! Sapphire limits inline map literals (like { a: 1, b: 2 }) to 255 elements for performance reasons.", "");
        error->add_suggestion("[Hint] Try breaking this initialization into multiple steps, or use a loop to populate your map dynamically instead of doing it all at once.", "");
    } else if (message == "Cannot have more than 255 elements in an array literal block.") {
        error->add_suggestion("Whoa, that's a huge array! Sapphire limits inline array literals (like [1, 2, 3]) to 255 elements for compiler efficiency.", "");
        error->add_suggestion("[Hint] If you need an array this big, try initializing it empty and filling it inside a loop using `.push()`, or split the literal into smaller chunks.", "");
    } else if (message == "Cannot reassign a constant variable.") {
        error->add_suggestion("You're trying to change the value of a constant. Constants (declared with 'const') are completely immutable after they are created.", "");
        error->add_suggestion("[Hint] If you intend to change this variable's value later in the code, you must declare it using 'var' instead of 'const'.", "");
    } else if (message == "Cannot use 'break' outside of a loop.") {
        error->add_suggestion("The 'break' keyword is designed exclusively to escape out of loops (like 'for' or 'while').", "");
        error->add_suggestion("[Hint] If you are trying to exit a function early, use 'return' instead. If you're inside an if-statement and just want to skip code, adjust your logic or use early returns.", "");
    } else if (message == "Cannot use 'continue' outside of a loop.") {
        error->add_suggestion("The 'continue' keyword tells the program to skip to the next iteration of a loop.", "");
        error->add_suggestion("[Hint] It seems you placed it outside of a 'for' or 'while' loop. If you want to skip logic inside a function, consider returning early or using an if-else block.", "");
    } else if (message == "Can't have more than 255 arguments.") {
        error->add_suggestion("You are passing too many arguments to this function call. The VM has a hard limit of 255 arguments per call.", "");
        error->add_suggestion("[Hint] This is often a sign that you should group related parameters together. Consider passing a single Map or an Array containing all this data instead!", "");
    } else if (message == "Can't have more than 255 parameters.") {
        error->add_suggestion("This function definition has way too many parameters! The limit is 255.", "");
        error->add_suggestion("[Hint] Clean up your function signature. Create a configuration class or a map object to bundle these parameters together into a single parameter.", "");
    } else if (message == "Can't read local variable in its own initializer.") {
        error->add_suggestion("You are trying to use a variable's name to calculate its own initial value.", "");
        error->add_suggestion("[Hint] For example, 'var a = a + 1;' doesn't work because 'a' doesn't exist yet. Make sure you're referencing a different, already-declared variable.", "");
    } else if (message == "Declarations must be initialized.") {
        error->add_suggestion("You declared a variable but didn't give it a starting value.", "");
        error->add_suggestion("[Hint] Variables must be explicitly initialized. E.g., 'var my_var = 0;' or 'var my_var = nil;' if you intend for it to be empty.", "");
    } else if (message == "Expect a method or field declaration inside class body.") {
        error->add_suggestion("You placed a loose expression or statement directly inside a class body.", "");
        error->add_suggestion("[Hint] Classes can only contain method functions and property fields. If you want to run arbitrary code (like math or function calls), it must be inside a method (like a constructor).", "");
    } else if (message == "Expect 'case' or 'default' inside switch statement.") {
        error->add_suggestion("Your switch block doesn't seem to contain any valid cases.", "");
        error->add_suggestion("[Hint] Make sure your switch body consists entirely of 'case X:' blocks and optionally a 'default:' block.", "");
    } else if (message == "Expect identifier after 'class' keyword.") {
        error->add_suggestion("Every class needs a name! You used the 'class' keyword but didn't provide an identifier.", "");
        error->add_suggestion("[Hint] Example: 'class User { ... }'. The name must start with a letter or underscore.", "");
    } else if (message == "Expect string literal or plugin name after 'import'.") {
        error->add_suggestion("The 'import' keyword requires you to specify what file or plugin you want to load.", "");
        error->add_suggestion("[Hint] Make sure you provide a string literal. Example: import \"math\"; or import \"./my_script.sp\";", "");
    } else if (message == "Expected expression.") {
        error->add_suggestion("The compiler hit a dead end because it expected a value here.", "");
        error->add_suggestion("[Hint] Check if you left an operator hanging (like 'var a = ;') or if you used an invalid token where a number, string, or variable should be.", "");
    } else if (message == "Incompatible types for assignment.") {
        error->add_suggestion("You're trying to stuff a value of one type into a target that expects a completely different type.", "");
        error->add_suggestion("[Hint] Check the type of the value you are returning or assigning, and consider explicitly converting it if necessary.", "");
    } else if (message == "Invalid time unit. Expected 's' or 'ms'.") {
        error->add_suggestion("You used a time literal suffix that the language doesn't recognize.", "");
        error->add_suggestion("[Hint] The valid time units are 's' (seconds) and 'ms' (milliseconds). Example: '100ms' or '2s'.", "");
    } else if (message == "Jump is too long to be encoded.") {
        error->add_suggestion("The code block inside this 'if' or 'while' is gigantically huge! The compiler's jump instructions have size limits.", "");
        error->add_suggestion("[Hint] This is a great opportunity to refactor. Split the contents of this large block into smaller, separate helper functions.", "");
    } else if (message == "Loop body too large.") {
        error->add_suggestion("The contents inside this loop are too large to be compiled into a single bytecode chunk.", "");
        error->add_suggestion("[Hint] Keep your loops lean! Extract the heavy logic inside the loop into a separate function and just call that function from within the loop.", "");
    } else if (message == "Numeric value out of bounds.") {
        error->add_suggestion("The number you wrote is either too large or too small to be safely stored by the engine's memory limits.", "");
        error->add_suggestion("[Hint] Try representing the value differently or check if you made a typo with too many zeros.", "");
    } else if (message == "'super' can only be used inside a class method.") {
        error->add_suggestion("The 'super' keyword is used to access methods from a parent class, but you are using it in the wild.", "");
        error->add_suggestion("[Hint] Make sure 'super' is only called from within a method of a class that inherits from another class (using the '<' symbol).", "");
    } else if (message == "The '+' operator requires two numbers or two strings.") {
        error->add_suggestion("You're trying to add two different types together (like adding a string to a number).", "");
        error->add_suggestion("[Hint] Sapphire doesn't do implicit coercion here to avoid bugs. Convert the number explicitly to a string using `to_string()` before concatenating.", "");
    } else if (message == "'this' can only be used inside a class method.") {
        error->add_suggestion("The 'this' keyword refers to the current instance of an object, but you used it outside of any object context.", "");
        error->add_suggestion("[Hint] Do not use 'this' in global scope scripts or standalone functions. It only belongs inside class methods.", "");
    } else if (message == "Too many constants in one chunk.") {
        error->add_suggestion("This specific file or block has too many distinct string, number, or object literals.", "");
        error->add_suggestion("[Hint] The limit is around 65535 constants. Try splitting your script into multiple files or modules to distribute the load.", "");
    } else if (message == "Too many local variables in a function." || message == "Too many local variables in function.") {
        error->add_suggestion("You've declared more local variables in a single function than the VM can comfortably track.", "");
        error->add_suggestion("[Hint] This is usually a sign that a function is doing too much work. Try splitting it, or group related variables into a single Array or Map.", "");
    } else if (message == "Expect '(' after 'if'.") {
        error->add_suggestion("In Sapphire, 'if' statements require their conditional expressions to be wrapped in parentheses.", "");
        error->add_suggestion("[Hint] E.g., change `if x > 0` to `if (x > 0)`.", "");
    } else if (message == "Expect '(' after 'for'.") {
        error->add_suggestion("A 'for' loop requires its clauses to be wrapped in parentheses.", "");
        error->add_suggestion("[Hint] E.g., change `for var i = 0; i < 10; i++` to `for (var i = 0; i < 10; i++)`.", "");
    } else if (message == "Expect '(' after 'while'.") {
        error->add_suggestion("A 'while' loop requires its condition to be wrapped in parentheses.", "");
        error->add_suggestion("[Hint] E.g., change `while true` to `while (true)`.", "");
    } else if (message == "Expect ')' after condition.") {
        error->add_suggestion("It seems you forgot to close the parentheses at the end of a conditional check.", "");
        error->add_suggestion("[Hint] Make sure every '(' inside the condition has a matching ')'.", "");
    } else if (message == "Expect '}' after block.") {
        error->add_suggestion("A code block `{ ... }` was opened but never closed.", "");
        error->add_suggestion("[Hint] Check your indentation. It's very likely you're missing a closing brace '}' at the end of a function, if-statement, or class.", "");
    } else if (message == "Expect '{' before function body.") {
        error->add_suggestion("Function bodies must be enclosed within curly braces.", "");
        error->add_suggestion("[Hint] Make sure you declare functions like this: `function myFunc() { ... }`.", "");
    } else if (message == "Expect '{' before class body.") {
        error->add_suggestion("Class bodies must start with an opening curly brace.", "");
        error->add_suggestion("[Hint] Example: `class Player { ... }`.", "");
    } else if (message == "Expect variable name.") {
        error->add_suggestion("You used a variable declaration keyword (like 'var' or 'const') but forgot to name the variable.", "");
        error->add_suggestion("[Hint] Identifiers must start with a letter or underscore. E.g., `var count = 0;`.", "");
    } else if (message == "Expect property name after '.'.") {
        error->add_suggestion("You placed a dot '.' to access a property, but didn't provide a valid property name after it.", "");
        error->add_suggestion("[Hint] Check if you left a trailing dot by accident, like `user. ` instead of `user.name`.", "");
    } else if (message == "Expect ';' after return statement.") {
        error->add_suggestion("Return statements are complete expressions and must be terminated with a semicolon.", "");
        error->add_suggestion("[Hint] Simply add a ';' right after the value you are returning.", "");
    } else if (message == "Expected ';' after variable declaration.") {
        error->add_suggestion("You declared a variable but didn't terminate the statement properly.", "");
        error->add_suggestion("[Hint] Every `var` or `const` declaration must end with a semicolon ';'.", "");
    } else if (message == "Expect '(' after function name.") {
        error->add_suggestion("When declaring a function, its parameter list must be enclosed in parentheses immediately after its name.", "");
        error->add_suggestion("[Hint] Even if the function takes no arguments, you must include empty parentheses. Example: `function start()`.", "");
    }
}

#include "../vm/vm.h"

void inject_runtime_hints(const std::string& message, std::shared_ptr<SapphireError>& error, void* vm_ptr) {
    if (message == "Stack overflow." || message == "Runtime Error: Stack overflow.") {
        error->add_suggestion("Your program exceeded the maximum call stack limit (usually because of too many nested calls).", "");
        VM* vm = reinterpret_cast<VM*>(vm_ptr);
        if (vm->frame_count > 0) {
            std::string func_name = "<anonymous>";
            if (vm->frames[vm->frame_count - 1].function != nullptr && vm->frames[vm->frame_count - 1].function->name != nullptr) {
                func_name = vm->frames[vm->frame_count - 1].function->name->chars;
            }
            error->add_suggestion("[Hint] The function '" + func_name + "' was running when it crashed. It might be stuck in an infinite recursion loop without a base case! Check its logic.", "");
        }
    } else if (message == "Stack underflow." || message == "Runtime Error: Stack underflow.") {
        error->add_suggestion("The virtual machine tried to pop a value from the stack, but the stack was completely empty.", "");
        error->add_suggestion("[Hint] This usually points to a compiler bug where expressions don't leave the expected number of values, or a bad native function.", "");
    }
}
