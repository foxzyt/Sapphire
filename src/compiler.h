#ifndef SAPPHIRE_COMPILER_H
#define SAPPHIRE_COMPILER_H

#include <string>
#include <vector>
#include <map>
#include <functional>

#include "object.h" // Para ObjFunction
#include "tokens.h" // Para Token e TokenType

// Declaração antecipada (Forward Declaration) para a VM.
// Só precisamos saber que o tipo existe, pois a função 'compile' usa um ponteiro para ele.
class VM;

// A struct 'Local' é usada exclusivamente pelo Compiler.
struct Local {
    Token name;
    int depth;
    TokenType type;
};

// A classe principal do Compilador.
class Compiler {
public:
    // Ponteiro para o compilador que envolve este (para escopos de função aninhados).
    Compiler* enclosing;
    // A função que este compilador está compilando.
    ObjFunction* function;

    int local_count;
    int scope_depth;
    // Array para rastrear variáveis locais no escopo atual.
    Local locals[256];

    // Mapa para rastrear tipos de variáveis globais (para verificação de tipo).
    std::map<std::string, TokenType> global_types;
    // O tipo de retorno esperado para a função atual.
    TokenType function_return_type;

    // Construtor e inicializador.
    Compiler(ObjFunction* func);
    void init(ObjFunction* func);
};

// A função principal exposta pelo módulo do compilador.
// Ela pega o código fonte e retorna uma função compilada (ou nullptr em caso de erro).
ObjFunction* compile(VM* vm, const std::string& source);

#endif // SAPPHIRE_COMPILER_H