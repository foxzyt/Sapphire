#ifndef SAPPHIRE_OPCODES_H
#define SAPPHIRE_OPCODES_H

#include <cstdint>
#include "tokens.h"

enum OpCode : uint8_t {
    // Constantes e Literais
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,

    // Stack
    OP_POP,

    // Variáveis e Propriedades
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,

    // Arrays
    OP_BUILD_ARRAY,
    OP_GET_SUBSCRIPT,
    OP_SET_SUBSCRIPT,

    // Operadores Lógicos e de Comparação
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_NOT,

    // Operadores Aritméticos
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,

    // Statements e Controle de Fluxo
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_RETURN = 31,
    OP_IMPORT = 32,
    OP_MAKE_NAMED_ARG = 33,
};

#endif //SAPPHIRE_OPCODES_H
