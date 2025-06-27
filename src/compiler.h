#ifndef SAPPHIRE_COMPILER_H
#define SAPPHIRE_COMPILER_H

#include "object.h"
#include "vm.h"
#include "tokens.h" // Incluído para a struct Token
#include <functional>
#include <string>
#include <map>

// Precedência dos operadores para o Pratt Parser
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

// Assinatura para as funções de parsing do Pratt Parser
using PrefixParseFn = std::function<TokenType(bool can_assign)>;

using InfixParseFn = std::function<TokenType(TokenType left_type, bool can_assign)>;

// Estrutura para uma regra de parsing
struct ParseRule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
};

// Estrutura para rastrear uma variável local no momento da compilação.
struct Local {
    Token name;
    int depth;
    TokenType type; 
};

// A classe Compiler agora é uma classe de estado, gerenciada pelo Parser.
class Compiler {
public:
    Compiler* enclosing = nullptr;
    ObjFunction* function = nullptr;

    Local locals[256];
    int local_count = 0;
    int scope_depth = 0;

    std::map<std::string, TokenType> global_types;

    Compiler(ObjFunction* func);
    TokenType function_return_type;
    void init(ObjFunction* func);
};

// A função principal que inicia todo o processo de compilação.
ObjFunction* compile(const std::string& source);

#endif //SAPPHIRE_COMPILER_H