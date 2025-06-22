#ifndef SAPPHIRE_TOKENS_H
#define SAPPHIRE_TOKENS_H

#include <string> // A correção crucial!

enum class TokenType {
    // Operadores
    PLUS, MINUS, STAR, SLASH, EQUAL,
    BANG, BANG_EQUAL,
    EQUAL_EQUAL,
    GREATER, GREATER_EQUAL,
    LESS, LESS_EQUAL,

    // Pontuação
    LEFT_PAREN, RIGHT_PAREN,
    LEFT_BRACE, RIGHT_BRACE,
    SEMICOLON,

    // Palavras-chave
    VAR, PRINT, IF, ELSE,
    TRUE, FALSE, NIL,
    AND, OR,

    // Literais
    NUMBER, STRING, IDENTIFIER,

    // Controle
    END_OF_FILE, ILLEGAL
};

struct Token {
    TokenType type;
    std::string literal;
};

#endif //SAPPHIRE_TOKENS_H