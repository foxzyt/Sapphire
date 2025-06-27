#ifndef SAPPHIRE_VALUE_H
#define SAPPHIRE_VALUE_H

#include <string>
#include <variant>
#include <iostream>
#include <memory>
#include <vector>

// Declarações antecipadas para que os tipos se conheçam sem criar ciclos.
struct Obj;
struct SapphireArray;
struct SapphireValue;

// O tipo de variant interno que guarda os dados.
using VariantValue = std::variant<
    std::monostate, 
    bool, 
    double, 
    Obj*, // Ponteiro para qualquer tipo de objeto (string, função, etc.)
    std::shared_ptr<SapphireArray> 
>;

// A nossa struct de valor principal.
struct SapphireValue {
    VariantValue _value;

    // Construtores para facilitar a criação de valores.
    SapphireValue() : _value(std::monostate{}) {}
    SapphireValue(std::monostate v) : _value(v) {}
    SapphireValue(bool v) : _value(v) {}
    SapphireValue(double v) : _value(v) {}
    SapphireValue(Obj* v) : _value(v) {}
    SapphireValue(std::shared_ptr<SapphireArray> v) : _value(v) {}
};

// A struct que define um array.
// Agora ela pode usar SapphireValue porque o tipo já é conhecido.
struct SapphireArray {
    std::vector<SapphireValue> elements;
};

// Declarações das nossas funções auxiliares.
void print_value(const SapphireValue& value);
bool is_falsey(const SapphireValue& value);
const char* get_value_type_name(const SapphireValue& value);

#endif //SAPPHIRE_VALUE_H