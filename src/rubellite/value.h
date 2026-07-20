#ifndef SAPPHIRE_VALUE_H
#define SAPPHIRE_VALUE_H

#include <string>
#include <variant>
#include <iostream>
#include <memory>
#include <vector>
#include <list>
#include <unordered_map>
#include <cstdint>

struct Obj;
struct ObjArray;
struct ObjLRU;

enum class ValType : uint8_t {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ
};

struct SapphireValue {
    ValType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;

    SapphireValue() : type(ValType::VAL_NIL) { as.number = 0.0; }
    SapphireValue(bool v) : type(ValType::VAL_BOOL) { as.boolean = v; }
    SapphireValue(double v) : type(ValType::VAL_NUMBER) { as.number = v; }
    SapphireValue(Obj* v) : type(ValType::VAL_OBJ) { as.obj = v; }
};

void print_value(const SapphireValue& value);
bool values_equal(const SapphireValue& a, const SapphireValue& b);
bool is_falsey(const SapphireValue& value);
const char* get_value_type_name(const SapphireValue& value);

#endif
