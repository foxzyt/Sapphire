#include "utils.h"
#include "tokens.h"
#include <fstream>
#include <sstream>
#include <iostream>

using enum TokenType;

std::string load_file_as_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        // Retornar string vazia para a VM lidar com o erro
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool check_for_soft_mode(const std::string& source) {
    return source.find("bool config_soft_mode = true") != std::string::npos;
}
