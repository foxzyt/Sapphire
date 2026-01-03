#include "bytecode_io.h"
#include <iostream>
#include <vector>
#include <variant>
#include "chunk.h"
#include "value.h"
#include "tokens.h"

using enum TokenType;

// Fun��es de escrita (agora aceitam std::ostream&)
void write_u8(std::ostream& out, uint8_t value) {
    out.put(value);
}

void write_u16(std::ostream& out, uint16_t value) {
    out.put((value >> 8) & 0xFF);
    out.put(value & 0xFF);
}

void write_u32(std::ostream& out, uint32_t value) {
    out.put((value >> 24) & 0xFF);
    out.put((value >> 16) & 0xFF);
    out.put((value >> 8) & 0xFF);
    out.put(value & 0xFF);
}

void write_double(std::ostream& out, double value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(double));
}

void write_string(std::ostream& out, const std::string& str) {
    write_u16(out, str.length());
    out.write(str.c_str(), str.length());
}

// serialize_sapphire_value agora aceita std::ostream&
void serialize_sapphire_value(std::ostream& out, VM* vm, const SapphireValue& value) {
    if (std::holds_alternative<std::monostate>(value._value)) {
        write_u8(out, 0);
    } else if (std::holds_alternative<bool>(value._value)) {
        write_u8(out, 1);
        write_u8(out, std::get<bool>(value._value) ? 1 : 0);
    } else if (std::holds_alternative<double>(value._value)) {
        write_u8(out, 2);
        write_double(out, std::get<double>(value._value));
    } else if (std::holds_alternative<Obj*>(value._value)) {
        Obj* obj = std::get<Obj*>(value._value);
        switch (obj->type) {
            case OBJ_STRING: {
                write_u8(out, 3);
                ObjString* str_obj = static_cast<ObjString*>(obj);
                write_string(out, str_obj->chars);
                break;
            }
            case OBJ_FUNCTION: {
                write_u8(out, 4);

                ObjFunction* func_obj = static_cast<ObjFunction*>(obj);

                bool has_name = (func_obj->name != nullptr);
                write_u8(out, has_name ? 1 : 0);
                if (has_name) {
                    write_string(out, func_obj->name->chars);
                }
                write_u8(out, func_obj->arity);
                write_u16(out, func_obj->chunk.constants.size());
                for (const auto& constant : func_obj->chunk.constants) {
                    serialize_sapphire_value(out, vm, constant);
                }
                write_u32(out, func_obj->chunk.code.size());
                out.write(reinterpret_cast<const char*>(func_obj->chunk.code.data()), func_obj->chunk.code.size());
                
                break;
            }
            case OBJ_CLASS: {
                write_u8(out, 5);
                ObjClass* klass_obj = static_cast<ObjClass*>(obj);
                write_string(out, klass_obj->name->chars);
                write_u16(out, klass_obj->methods.size());
                for (const auto& pair : klass_obj->methods) {
                    write_string(out, pair.first);
                    ObjClosure* closure_obj = static_cast<ObjClosure*>(pair.second);
                    serialize_sapphire_value(out, vm, SapphireValue(closure_obj->function));
                }
                break;
            }
            case OBJ_NATIVE: {
                write_u8(out, 6);
                ObjNative* native_obj = static_cast<ObjNative*>(obj);
                write_string(out, native_obj->name->chars);
                break;
            }
            case OBJ_CLOSURE: {
                write_u8(out, 7);
                ObjClosure* closure_obj = static_cast<ObjClosure*>(obj);
                serialize_sapphire_value(out, vm, SapphireValue(closure_obj->function));
                break;
            }
            case OBJ_BOUND_METHOD: {
                std::cerr << "Warning: Attempting to serialize an OBJ_BOUND_METHOD. This is usually an error." << std::endl;
                write_u8(out, 0);
                break;
            }
            case OBJ_INSTANCE: {
                ObjInstance* instance = static_cast<ObjInstance*>(obj);
                for (const auto& pair : vm->globals) {
                    if (std::holds_alternative<Obj*>(pair.second._value) && std::get<Obj*>(pair.second._value) == instance) {
                        write_u8(out, 100); // 100 = Marcador para NATIVE_INSTANCE_REF
                        write_string(out, pair.first);
                        return; // Saímos da função
                    }
                }
                std::cerr << "Warning: Attempting to serialize a non-native OBJ_INSTANCE. This is not supported." << std::endl;
                write_u8(out, 0); // nil
                break;
            }
        }
    } else if (std::holds_alternative<std::shared_ptr<SapphireArray>>(value._value)) {
        write_u8(out, 8);
        auto array_ptr = std::get<std::shared_ptr<SapphireArray>>(value._value);
        write_u32(out, array_ptr->elements.size());
        for (const auto& element : array_ptr->elements) {
            serialize_sapphire_value(out, vm, element);
        }
    } else {
        std::cerr << "Error: Unknown SapphireValue type during serialization." << std::endl;
        write_u8(out, 0);
    }
}

void serialize_function(ObjFunction* function, VM* vm, const std::string& output_path) {
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return;
    }

    out.write("SBC2", 4);
    write_u8(out, 1);
    write_u8(out, vm->soft_mode ? 1 : 0);

    std::vector<std::pair<std::string, SapphireValue>> serializable_globals;
    for (const auto& pair : vm->globals) {
        if (std::holds_alternative<Obj*>(pair.second._value) && std::get<Obj*>(pair.second._value)->type == OBJ_NATIVE) {
            continue;
        }
        serializable_globals.push_back(pair);
    }

    write_u16(out, serializable_globals.size());
    for (const auto& pair : serializable_globals) {
        write_string(out, pair.first);
        serialize_sapphire_value(out, vm, pair.second);
    }

    serialize_sapphire_value(out, vm, SapphireValue(function));
}

// Fun��es de leitura (agora aceitam std::istream&)
uint8_t read_u8(std::istream& in) {
    return in.get();
}

uint16_t read_u16(std::istream& in) {
    uint8_t byte1 = in.get();
    uint8_t byte2 = in.get();
    return (uint16_t)((byte1 << 8) | byte2);
}

uint32_t read_u32(std::istream& in) {
    uint8_t byte1 = in.get();
    uint8_t byte2 = in.get();
    uint8_t byte3 = in.get();
    uint8_t byte4 = in.get();
    return (uint32_t)((byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4);
}

double read_double(std::istream& in) {
    double value;
    in.read(reinterpret_cast<char*>(&value), sizeof(double));
    return value;
}

std::string read_string(std::istream& in) {
    uint16_t length = read_u16(in);
    std::string str(length, '\0');
    in.read(&str[0], length);
    return str;
}

// deserialize_sapphire_value agora aceita std::istream&
SapphireValue deserialize_sapphire_value(std::istream& in, VM* vm) {
    uint8_t type_byte = read_u8(in);
    switch (type_byte) {
        case 0: return {};
        case 1: return (bool)read_u8(in);
        case 2: return read_double(in);
        case 3: return new_string(vm, read_string(in));
        case 4: {
            ObjFunction* func = new_function(vm);
            
            bool has_name = (read_u8(in) == 1);
            if (has_name) {
                func->name = new_string(vm, read_string(in));
            } else {
                func->name = nullptr;
            }
            func->arity = read_u8(in);

            uint16_t constant_count = read_u16(in);
            func->chunk.constants.reserve(constant_count);
            for (int i = 0; i < constant_count; ++i) {
                func->chunk.constants.push_back(deserialize_sapphire_value(in, vm)); // Recursivo
            }

            uint32_t code_size = read_u32(in);
            func->chunk.code.resize(code_size);
            in.read(reinterpret_cast<char*>(func->chunk.code.data()), code_size);

            return func;
        }
        case 5: {
            ObjClass* klass = new_class(vm, new_string(vm, read_string(in)));
            uint16_t method_count = read_u16(in);
            for (int i = 0; i < method_count; ++i) {
                std::string method_name = read_string(in);
                SapphireValue method_val = deserialize_sapphire_value(in, vm);
                if (std::holds_alternative<Obj*>(method_val._value) && std::get<Obj*>(method_val._value)->type == OBJ_FUNCTION) {
                    klass->methods[method_name] = new_closure(vm, static_cast<ObjFunction*>(std::get<Obj*>(method_val._value)));
                } else {
                    std::cerr << "Error: Expected function for class method '" << method_name << "' during deserialization." << std::endl;
                }
            }
            return klass;
        }
        case 6: {
            std::string native_name = read_string(in);
            auto it = vm->globals.find(native_name);
            if (it != vm->globals.end() && std::holds_alternative<Obj*>(it->second._value) && std::get<Obj*>(it->second._value)->type == OBJ_NATIVE) {
                return it->second;
            } else {
                std::cerr << "Error: Native function '" << native_name << "' not found during deserialization. This might cause issues if it's a constant." << std::endl;
                return {};
            }
        }
        case 7: {
            SapphireValue func_val = deserialize_sapphire_value(in, vm);
            if (std::holds_alternative<Obj*>(func_val._value) && std::get<Obj*>(func_val._value)->type == OBJ_FUNCTION) {
                ObjFunction* func = static_cast<ObjFunction*>(std::get<Obj*>(func_val._value));
                return new_closure(vm, func);
            } else {
                std::cerr << "Error: Expected function for closure during deserialization." << std::endl;
                return {};
            }
        }
        case 8: {
            auto array_obj = std::make_shared<SapphireArray>();
            uint32_t element_count = read_u32(in);
            array_obj->elements.reserve(element_count);
            for (int i = 0; i < element_count; ++i) {
                array_obj->elements.push_back(deserialize_sapphire_value(in, vm)); // Recursivo
            }
            return array_obj;
        }
        case 100: {
            std::string global_name = read_string(in);
            auto it = vm->globals.find(global_name);
            if (it != vm->globals.end()) {
                return it->second;
            }
            std::cerr << "Error: Native global '" << global_name << "' not found during deserialization." << std::endl;
            return {};
        }
        default:
            std::cerr << "Error: Unknown SapphireValue type " << (int)type_byte << " during deserialization." << std::endl;
            return {};
    }
}

// deserialize_function (l� de um caminho de arquivo, mant�m std::ifstream)
ObjFunction* deserialize_function(VM* vm, const std::string& input_path) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        std::cerr << "Error: Could not open bytecode file: " << input_path << std::endl;
        return nullptr;
    }

    char header[4];
    in.read(header, 4);
    if (std::string(header, 4) != "SBC2") {
        std::cerr << "Error: Invalid bytecode file header or unsupported version. Expected 'SBC2'." << std::endl;
        return nullptr;
    }

    uint8_t version = read_u8(in);
    if (version != 1) {
        std::cerr << "Error: Unsupported bytecode file version: " << (int)version << ". Expected 1." << std::endl;
        return nullptr;
    }

    uint8_t soft_mode_flag = read_u8(in);
    vm->soft_mode = (soft_mode_flag == 1);

    uint16_t global_count = read_u16(in);
    for (int i = 0; i < global_count; ++i) {
        std::string global_name = read_string(in);
        SapphireValue global_value = deserialize_sapphire_value(in, vm);
        vm->globals[global_name] = global_value;
    }

    SapphireValue main_func_val = deserialize_sapphire_value(in, vm);
    if (std::holds_alternative<Obj*>(main_func_val._value) && std::get<Obj*>(main_func_val._value)->type == OBJ_FUNCTION) {
        return static_cast<ObjFunction*>(std::get<Obj*>(main_func_val._value));
    } else {
        std::cerr << "Error: Expected a function as the main script entry point." << std::endl;
        return nullptr;
    }
}

// deserialize_function_from_stream (l� de um stream gen�rico, agora aceita std::istream)
ObjFunction* deserialize_function_from_stream(VM* vm, std::istream& in) {
    char header[4];
    in.read(header, 4);
    if (std::string(header, 4) != "SBC2") {
        std::cerr << "Error: Invalid bytecode stream header or unsupported version. Expected 'SBC2'." << std::endl;
        return nullptr;
    }

    uint8_t version = read_u8(in);
    if (version != 1) {
        std::cerr << "Error: Unsupported bytecode stream version: " << (int)version << ". Expected 1." << std::endl;
        return nullptr;
    }

    uint8_t soft_mode_flag = read_u8(in);
    vm->soft_mode = (soft_mode_flag == 1);

    uint16_t global_count = read_u16(in);
    for (int i = 0; i < global_count; ++i) {
        std::string global_name = read_string(in);
        SapphireValue global_value = deserialize_sapphire_value(in, vm);
        vm->globals[global_name] = global_value;
    }

    SapphireValue main_func_val = deserialize_sapphire_value(in, vm);
    if (std::holds_alternative<Obj*>(main_func_val._value) && std::get<Obj*>(main_func_val._value)->type == OBJ_FUNCTION) {
        return static_cast<ObjFunction*>(std::get<Obj*>(main_func_val._value));
    } else {
        std::cerr << "Error: Expected a function as the main script entry point from stream." << std::endl;
        return nullptr;
    }
}
