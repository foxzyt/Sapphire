#ifndef SAPPHIRE_CHUNK_H
#define SAPPHIRE_CHUNK_H

#include "opcodes.h"
#include "value.h"
#include <vector>
#include <cstdint>

struct Chunk {
    std::vector<uint8_t> code;          // O bytecode em si. Uma lista de instruções.
    std::vector<SapphireValue> constants; // A "piscina" de constantes.

    // Função auxiliar para escrever um byte no chunk.
    void write(uint8_t byte) {
        code.push_back(byte);
    }

    // Função para adicionar uma constante à piscina e retornar seu índice.
    // Retorna 'int' para suportar mais de 256 constantes no futuro.
    int add_constant(const SapphireValue& value) {
        constants.push_back(value);
        return constants.size() - 1;
    }
};

#endif //SAPPHIRE_CHUNK_H