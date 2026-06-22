#ifndef SAPPHIRE_BYTECODE_IO_H
#define SAPPHIRE_BYTECODE_IO_H

#include <string>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "object.h"
#include "vm.h"
#include "tokens.h"

void write_u8(std::ostream& out, uint8_t value);
void write_u16(std::ostream& out, uint16_t value);
void write_u32(std::ostream& out, uint32_t value);
void write_double(std::ostream& out, double value);
void write_string(std::ostream& out, const std::string& str);
void serialize_sapphire_value(std::ostream& out, VM* vm, const SapphireValue& value);
void serialize_function(ObjFunction* function, VM* vm, const std::string& output_path);

uint8_t read_u8(std::istream& in);
uint16_t read_u16(std::istream& in);
uint32_t read_u32(std::istream& in);
double read_double(std::istream& in);
std::string read_string(std::istream& in);
SapphireValue deserialize_sapphire_value(std::istream& in, VM* vm);

ObjFunction* deserialize_function_from_stream(VM* vm, std::istream& in);
ObjFunction* deserialize_function(VM* vm, const std::string& input_path);

#endif // SAPPHIRE_BYTECODE_IO_H
