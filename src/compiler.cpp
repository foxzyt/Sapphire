#include "compiler.h"
#include "parser.h" // O parser.cpp conterá a implementação do parser.
#include <iostream>

// Construtor e inicializador do Compiler
Compiler::Compiler(ObjFunction* func) {
    init(func);
}

void Compiler::init(ObjFunction* func) {
    this->enclosing = nullptr;
    this->function = func;
    this->local_count = 0;
    this->scope_depth = 0;

    // A VM reserva o slot 0 da pilha para o uso interno da função.
    Local* local = &locals[local_count++];
    local->depth = 0;
    local->name.literal = ""; // Vazio para o corpo do script ou 'this' para métodos de classe no futuro
}


// A função de compilação agora está em seu próprio arquivo.
// Ela será chamada pelo parser.h
ObjFunction* compile(const std::string& source) {
    Lexer lexer(source);
    Compiler compiler(new_function());

    // Inicializa o parser (que será definido em parser.cpp)
    // e passa a ele o lexer e o compilador.
    Parser parser(lexer, &compiler);

    while (!parser.match(TokenType::END_OF_FILE)) {
        parser.declaration();
    }
    
    ObjFunction* main_function = compiler.function;
    parser.emit_return(); // Garante que o script principal sempre retorne.

    // Retorna nullptr se houve um erro de compilação.
    return parser.had_error ? nullptr : main_function;
}