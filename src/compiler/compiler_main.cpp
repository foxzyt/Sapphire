#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "utils.h"
#include "bytecode_io.h"
#include "tokens.h"

using enum TokenType;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: sapphirec <input_file.sp> <output_file.sbc>" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];

    std::string source = load_file_as_string(input_path);
    if (source.empty()) {
        std::cerr << "Error: Could not read input file: " << input_path << std::endl;
        return 1;
    }

    bool soft_mode_enabled = check_for_soft_mode(source);

    VM vm; // Instancia a VM que será usada para a compilação
    vm.soft_mode = soft_mode_enabled; // Configura o soft mode na VM do compilador

    ObjFunction* main_function = compile(&vm, source);

    if (main_function == nullptr) {
        std::cerr << "Compilation error in input file." << std::endl;
        return 1;
    }

    // Chama a função de serialização que está definida em bytecode_io.cpp
    serialize_function(main_function, &vm, output_path);

    std::cout << "Compilation completed successfully! Bytecode saved in: " << output_path << std::endl;

    return 0;
}
