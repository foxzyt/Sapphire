#ifndef SAPPHIRE_LEXER_H
#define SAPPHIRE_LEXER_H

#include "tokens.h"
#include <vector>
#include <map>
#include <string>

class Lexer {
public:
    Lexer(const std::string& source);
    Token scan_token(); // <<< AGORA É PÚBLICO E RETORNA UM TOKEN

private:
    std::string source;
    int start = 0;
    int current = 0;
    int line = 1; // Rastreia a linha atual

    bool is_at_end();
    char advance();
    char peek();
    char peek_next();
    bool match(char expected);

    Token make_token(TokenType type);
    Token make_token(TokenType type, const std::string& literal);
    Token error_token(const std::string& message);
    Token string_token();
    Token number_token();
    Token identifier_token();
};

#endif // SAPPHIRE_LEXER_H