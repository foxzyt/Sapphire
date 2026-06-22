#include "compiler.h"
#include "parser.h"
#include "tokens.h"
#include "debug.h"
using enum TokenType;
#include <iostream>

Compiler::Compiler(ObjFunction* func) {
    init(func);
}

void Compiler::init(ObjFunction* func) {
    this->enclosing = nullptr;
    this->function = func;
    this->local_count = 0;
    this->scope_depth = 0;

    Local* local = &locals[local_count++];
    local->depth = 0;
    local->name.literal = "";
}

// A função de compilação agora está em seu próprio arquivo.
// Ela será chamada pelo parser.h
ObjFunction* compile(VM* vm, const std::string& source) {
    Lexer lexer(source);
    Compiler compiler(new_function(vm));

    Parser parser(lexer, &compiler, vm);

    // Continue a declarar até o fim do arquivo ou um erro grave
    while (!parser.match(TokenType::TOKEN_END_OF_FILE)) {
        parser.declaration();
    }

    ObjFunction* main_function = compiler.function;
    // Garanta que o emit_return esteja sempre no final do chunk
    // Adicionamos uma verificação para evitar múltiplos returns no final ou returns duplos
    if (main_function->chunk.code.empty() || main_function->chunk.code.back() != OP_RETURN) {
        parser.emit_return(); // Garante que o script principal sempre retorne algo.
    }

    #ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        // Renomeie 'compiler.function->name' se ele for nulo para o script principal
        std::string debug_name = (main_function->name != nullptr) ? main_function->name->chars : "<script>";
        disassemble_chunk(main_function->chunk, debug_name);
    }
    #endif

    // Retorna nullptr se houve um erro de compilação.
    return parser.had_error ? nullptr : main_function;
}
