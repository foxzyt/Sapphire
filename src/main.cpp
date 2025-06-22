#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "evaluator.h"
#include "lexer.h"
#include "parser.h"

// --- Função de Ajuda para Imprimir Nomes de Tokens ---
// Precisamos recriá-la aqui para nosso teste.
std::string token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::BANG: return "BANG";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::VAR: return "VAR";
        case TokenType::PRINT: return "PRINT";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NIL: return "NIL";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::ILLEGAL: return "ILLEGAL";
        default: return "DESCONHECIDO";
    }
}


Evaluator evaluator;

void run(const std::string& source) {
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.scan_tokens();

        // ======================================================
        //               INÍCIO DO CÓDIGO DE DEPURAÇÃO
        // ======================================================
        std::cout << "--- INICIO DA LISTA DE TOKENS ---" << std::endl;
        for (const auto& token : tokens) {
            std::cout << "Tipo: " << token_type_to_string(token.type)
                      << ", Literal: '" << token.literal << "'" << std::endl;
        }
        std::cout << "--- FIM DA LISTA DE TOKENS ---" << std::endl << std::endl;
        // ======================================================
        //                FIM DO CÓDIGO DE DEPURAÇÃO
        // ======================================================


        Parser parser(tokens);
        auto statements = parser.parse();
        evaluator.evaluate(statements);

    } catch (const std::runtime_error& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
    }
}

// ... as funções run_file e run_prompt permanecem as mesmas ...

void run_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Erro: Nao foi possivel abrir o arquivo '" << path << "'." << std::endl;
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    run(buffer.str());
}

void run_prompt() { /* ... */ }

int main(int argc, char* argv[]) {
    if (argc == 2) {
        run_file(argv[1]);
    } else {
        std::cout << "Uso: sapphire [caminho_do_script]" << std::endl;
    }
    return 0;
}