#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "vm.h"

static VM vm; // Instância única da VM

// Função para rodar um arquivo de script
static void run_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Erro: Nao foi possivel abrir o arquivo '" << path << "'." << std::endl;
        exit(74);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    vm.interpret(buffer.str());
}

// Função para o modo interativo (REPL)
static void repl() {
    std::cout << "Sapphire VM - Interactive Mode" << std::endl;
    std::string line;
    for (;;) {
        std::cout << "> ";
        if (!std::getline(std::cin, line) || line == "exit") {
            break;
        }
        vm.interpret(line);
    }
}

int main(int argc, char* argv[]) {
    std::cout << ">>>>>> TESTE DE COMPILACAO REALIZADO COM SUCESSO <<<<<<" << std::endl;
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        std::cerr << "Uso: sapphire [caminho_do_script]" << std::endl;
        return 64; // Código de erro para uso incorreto
    }

    return 0;
}