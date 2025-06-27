#include <iomanip>
#include <iostream>

#include "debug.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// --- Funções Auxiliares de Disassembly ---

static int simple_instruction(const char* name, int offset) {
    std::cout << name << std::endl;
    return offset + 1;
}

static int byte_instruction(const char* name, const Chunk& chunk, int offset) {
    uint8_t slot = chunk.code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int constant_instruction(const char* name, const Chunk& chunk, int offset) {
    uint8_t constant_index = chunk.code[offset + 1];
    printf("%-16s %4d '", name, constant_index);
    print_value(chunk.constants[constant_index]);
    printf("'\n");
    return offset + 2;
}

static int jump_instruction(const char* name, int sign, const Chunk& chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk.code[offset + 1] << 8);
    jump |= chunk.code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}


// --- Função Principal de Disassembly ---

int disassemble_instruction(const Chunk& chunk, int offset) {
    printf("%04d ", offset);

    uint8_t instruction = chunk.code[offset];
    switch (instruction) {
        case OP_CONSTANT:      return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:           return simple_instruction("OP_NIL", offset);
        case OP_TRUE:          return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:         return simple_instruction("OP_FALSE", offset);
        case OP_POP:           return simple_instruction("OP_POP", offset);
        case OP_GET_LOCAL:     return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:     return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:    return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL: return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:    return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_PROPERTY:  return constant_instruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:  return constant_instruction("OP_SET_PROPERTY", chunk, offset);
        case OP_GET_SUBSCRIPT: return simple_instruction("OP_GET_SUBSCRIPT", offset);
        case OP_SET_SUBSCRIPT: return simple_instruction("OP_SET_SUBSCRIPT", offset);
        case OP_EQUAL:         return simple_instruction("OP_EQUAL", offset);
        case OP_GREATER:       return simple_instruction("OP_GREATER", offset);
        case OP_LESS:          return simple_instruction("OP_LESS", offset);
        case OP_ADD:           return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:      return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:      return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:        return simple_instruction("OP_DIVIDE", offset);
        case OP_NOT:           return simple_instruction("OP_NOT", offset);
        case OP_NEGATE:        return simple_instruction("OP_NEGATE", offset);
        case OP_PRINT:         return simple_instruction("OP_PRINT", offset);
        case OP_JUMP:          return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:          return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:          return byte_instruction("OP_CALL", chunk, offset);
        case OP_CLOSURE:       return constant_instruction("OP_CLOSURE", chunk, offset);
        case OP_BUILD_ARRAY:   return byte_instruction("OP_BUILD_ARRAY", chunk, offset);
        case OP_RETURN:        return simple_instruction("OP_RETURN", offset);
        default:
            std::cout << "Instrucao desconhecida: " << (int)instruction << std::endl;
            return offset + 1;
    }
}

void disassemble_chunk(const Chunk& chunk, const std::string& name) {
    std::cout << "== " << name << " ==" << std::endl;
    for (size_t offset = 0; offset < chunk.code.size();) {
        offset = disassemble_instruction(chunk, offset);
    }
}

// --- Nova Função de Debug da Pilha ---

void debug_print_stack(VM* vm) {
    std::cout << "          ";
    for (SapphireValue* slot = vm->stack; slot < vm->stack_top; slot++) {
        std::cout << "[ ";
        print_value(*slot);
        std::cout << " ]";
    }
    std::cout << std::endl;
}