#ifndef SAPPHIRE_ENVIRONMENT_H
#define SAPPHIRE_ENVIRONMENT_H

#include "value.h"
#include "tokens.h"
#include <map>
#include <string>
#include <memory>
#include <stdexcept>

class Environment : public std::enable_shared_from_this<Environment> {
private:
    std::shared_ptr<Environment> enclosing;
    std::map<std::string, SapphireValue> values;

public:
    Environment() : enclosing(nullptr) {}
    Environment(std::shared_ptr<Environment> enclosing) : enclosing(std::move(enclosing)) {}

    void define(const std::string& name, const SapphireValue& value) {
        values[name] = value;
    }

    SapphireValue get(const Token& name) {
        if (values.count(name.literal)) {
            return values.at(name.literal);
        }

        if (enclosing != nullptr) {
            return enclosing->get(name);
        }

        throw std::runtime_error("Variavel nao definida: '" + name.literal + "'.");
    }

    void assign(const Token& name, const SapphireValue& value) {
        if (values.count(name.literal)) {
            values[name.literal] = value;
            return;
        }

        if (enclosing != nullptr) {
            enclosing->assign(name, value);
            return;
        }

        throw std::runtime_error("Variavel nao definida para atribuicao: '" + name.literal + "'.");
    }
};

#endif //SAPPHIRE_ENVIRONMENT_H