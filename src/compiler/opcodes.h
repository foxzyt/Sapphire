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
    OP_DUP,

    // Variáveis e Propriedades
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,

    // Arrays and Maps
    OP_BUILD_ARRAY,
    OP_BUILD_MAP,
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
    OP_MODULO,
    OP_NEGATE,
    
    // Bitwise Operadores
    OP_BITWISE_AND,
    OP_BITWISE_OR,
    OP_BITWISE_XOR,
    OP_BITWISE_NOT,
    OP_LEFT_SHIFT,
    OP_RIGHT_SHIFT,

    // Statements e Controle de Fluxo
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_RETURN,
    OP_IMPORT,
    OP_MAKE_NAMED_ARG,
    OP_INHERIT,
    OP_GET_SUPER,
    OP_SPAWN,
    OP_AWAIT,
    OP_ASYNC_CALL,
    
    // Exceptions
    OP_TRY_START,
    OP_TRY_END,
    OP_THROW,
};

#endif //SAPPHIRE_OPCODES_H
