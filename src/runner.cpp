#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "vm.h"
#include "object.h"
#include "utils.h"
#include "value.h"
#include "bytecode_io.h"
#include "tokens.h"

using enum TokenType;

ObjFunction* deserialize_function(VM* vm, const std::string& path);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: runner <file.sbc>" << std::endl;
        return 1;
    }

    std::string bytecode_path = argv[1];

    VM vm;
    ObjFunction* main_function = deserialize_function(&vm, bytecode_path);

    if (main_function == nullptr) {
        std::cerr << "Error: Could not load bytecode from file: " << bytecode_path << std::endl;
        return 1;
    }

    bool success = vm.call_and_run(main_function);

    if (success) {
        std::cout << "Script executed sucessfuly." << std::endl;
        std::cout << "Value returned by script: ";
        print_value(vm.pop());
        std::cout << std::endl;
    } else {
        std::cout << "Script ended with an error." << std::endl;
    }

    return 0;
}
