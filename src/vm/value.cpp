#include "value.h"
#include "object.h"
#include <iostream>
#include <cmath>

bool is_falsey(const SapphireValue& value) {
    return std::holds_alternative<std::monostate>(value._value) ||
           (std::holds_alternative<bool>(value._value) && !std::get<bool>(value._value));
}

const char* get_value_type_name(const SapphireValue& value) {
    if (std::holds_alternative<std::monostate>(value._value)) return "nil";
    if (std::holds_alternative<bool>(value._value)) return "boolean";
    if (std::holds_alternative<double>(value._value)) return "number";
    if (std::holds_alternative<Obj*>(value._value)) {
        // Assume que Obj tem um campo 'type'
        switch (std::get<Obj*>(value._value)->type) {
            case OBJ_STRING: return "string";
            case OBJ_FUNCTION: return "function";
            case OBJ_NATIVE: return "native function";
            default: return "object";
        }
    }
    return "unknown";
}

void print_value(const SapphireValue& value) {
    std::visit([&value](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            std::cout << "nil";
        } else if constexpr (std::is_same_v<T, bool>) {
            std::cout << (arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, double>) {
            double int_part;
            if (modf(arg, &int_part) == 0.0) {
                std::cout << static_cast<long long>(arg);
            } else {
                std::cout << arg;
            }
        } else if constexpr (std::is_same_v<T, Obj*>) {
            print_object(value);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SapphireArray>>) {
            std::cout << "[";
            for (size_t i = 0; i < arg->elements.size(); ++i) {
                print_value(arg->elements[i]);
                if (i < arg->elements.size() - 1) std::cout << ", ";
            }
            std::cout << "]";
        }
    }, value._value);
}