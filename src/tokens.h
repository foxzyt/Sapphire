#ifndef SAPPHIRE_TOKENS_H
#define SAPPHIRE_TOKENS_H

#include <string>

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
    LEFT_BRACKET, RIGHT_BRACKET,
    SEMICOLON,
    COMMA, DOT,

    // Palavras-chave
    PRINT, IF, ELSE,
    TRUE, FALSE, NIL,
    AND, OR,
    WHILE,
    FUNCTION,
    RETURN,
    IMPORT,
    INT, BOOL, STRING, DOUBLE, FLOAT,
    VOID, CLASS, THIS,

    // Literais
    NUMBER, STRING_LITERAL, IDENTIFIER,

    // Controle
    END_OF_FILE, ILLEGAL
};

struct Token {
    TokenType type;
    std::string literal;
    int line;
};

#endif //SAPPHIRE_TOKENS_H