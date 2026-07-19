#include "value.h"
#include "object.h"
#include <iostream>
#include <cmath>
bool values_equal(const SapphireValue& a, const SapphireValue& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case ValType::VAL_NIL: return true;
        case ValType::VAL_BOOL: return a.as.boolean == b.as.boolean;
        case ValType::VAL_NUMBER: return a.as.number == b.as.number;
        case ValType::VAL_OBJ: {
            if (a.as.obj == b.as.obj) return true;
            if (a.as.obj->type == OBJ_STRING && b.as.obj->type == OBJ_STRING) {
                return static_cast<ObjString*>(a.as.obj)->chars == static_cast<ObjString*>(b.as.obj)->chars;
            }
            return false;
        }
    }
    return false;
}

bool is_falsey(const SapphireValue& value) {
    return value.type == ValType::VAL_NIL ||
           (value.type == ValType::VAL_BOOL && !value.as.boolean);
}

const char* get_value_type_name(const SapphireValue& value) {
    if (value.type == ValType::VAL_NIL) return "nil";
    if (value.type == ValType::VAL_BOOL) return "boolean";
    if (value.type == ValType::VAL_NUMBER) return "number";
    if (value.type == ValType::VAL_OBJ) {
        switch (value.as.obj->type) {
            case OBJ_STRING: return "string";
            case OBJ_FUNCTION: return "function";
            case OBJ_NATIVE: return "native function";
            case OBJ_ARRAY: return "array";
            case OBJ_LRU: return "lru_cache";
            default: return "object";
        }
    }
    return "unknown";
}

void print_value(const SapphireValue& value) {
    if (value.type == ValType::VAL_NIL) {
        std::cout << "nil";
    } else if (value.type == ValType::VAL_BOOL) {
        std::cout << (value.as.boolean ? "true" : "false");
    } else if (value.type == ValType::VAL_NUMBER) {
        double int_part;
        if (modf(value.as.number, &int_part) == 0.0) {
            std::cout << static_cast<long long>(value.as.number);
        } else {
            std::cout << value.as.number;
        }
    } else if (value.type == ValType::VAL_OBJ) {
        print_object(value);
    }
}