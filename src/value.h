#ifndef SAPPHIRE_VALUE_H
#define SAPPHIRE_VALUE_H

#include <string>
#include <variant>
#include <iostream>

// Usamos std::variant para criar um "contêiner" que pode guardar diferentes tipos.
// std::monostate representa o nosso valor 'nil' (nulo).
using SapphireValue = std::variant<std::monostate, bool, double, std::string>;

// Função de ajuda para saber se um valor é "verdadeiro" em um if.
// Esta função é 'inline' para evitar erros de compilação quando incluída em múltiplos arquivos.
inline bool is_truthy(const SapphireValue& value) {
    // nil é falso
    if (std::holds_alternative<std::monostate>(value)) return false;
    // booleano false é falso
    if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
    // número 0 é falso
    if (std::holds_alternative<double>(value)) return std::get<double>(value) != 0;
    // string vazia é falsa
    if (std::holds_alternative<std::string>(value)) return !std::get<std::string>(value).empty();
    
    return false; // Por segurança
}

// Função de ajuda para imprimir qualquer SapphireValue no console.
inline void print_value(const SapphireValue& value) {
    if (std::holds_alternative<double>(value)) {
        std::cout << std::get<double>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        std::cout << std::get<std::string>(value);
    } else if (std::holds_alternative<bool>(value)) {
        std::cout << (std::get<bool>(value) ? "true" : "false");
    } else {
        std::cout << "nil";
    }
}

#endif //SAPPHIRE_VALUE_H