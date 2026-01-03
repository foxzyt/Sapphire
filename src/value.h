#ifndef SAPPHIRE_VALUE_H
#define SAPPHIRE_VALUE_H

#include <string>
#include <variant>
#include <iostream>
#include <memory>
#include <vector>

struct Obj;
struct SapphireArray;
struct SapphireValue;

using VariantValue = std::variant<
    std::monostate,
    bool,
    double,
    Obj*,
    std::shared_ptr<SapphireArray>
>;

struct SapphireValue {
    VariantValue _value;

    SapphireValue() : _value(std::monostate{}) {}
    SapphireValue(std::monostate v) : _value(v) {}
    SapphireValue(bool v) : _value(v) {}
    SapphireValue(double v) : _value(v) {}
    SapphireValue(Obj* v) : _value(v) {}
    SapphireValue(std::shared_ptr<SapphireArray> v) : _value(v) {}
};

struct SapphireArray {
    std::vector<SapphireValue> elements;
};

void print_value(const SapphireValue& value);
bool is_falsey(const SapphireValue& value);
const char* get_value_type_name(const SapphireValue& value);

#endif