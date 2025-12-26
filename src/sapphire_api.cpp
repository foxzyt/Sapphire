#include "sapphire_api.h"
#include "vm.h"
#include "compiler.h"
#include "bytecode_io.h"
#include <iostream>

bool compile_source_to_bytecode_file(const std::string& source_code, const std::string& output_path) {
    VM vm;

    ObjFunction* main_function = compile(&vm, source_code);

    if (main_function == nullptr) {
        std::cerr << "Compilation failed." << std::endl;
        return false;
    }

    serialize_function(main_function, &vm, output_path);
    return true;
}