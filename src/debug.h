#ifndef SAPPHIRE_DEBUG_H
#define SAPPHIRE_DEBUG_H

#include "chunk.h"
#include <string>

class VM;

void disassemble_chunk(const Chunk& chunk, const std::string& name);
int disassemble_instruction(const Chunk& chunk, int offset);
void debug_print_stack(VM* vm);

#endif //SAPPHIRE_DEBUG_H