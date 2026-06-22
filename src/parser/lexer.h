#ifndef SAPPHIRE_LEXER_H
#define SAPPHIRE_LEXER_H

#include "tokens.h"
#include <string>

class Lexer {
public:
    Lexer(const std::string& source);
    Token scan_token();
    const std::string& get_source() const { return source; }

private:
    Token make_token(TokenType type);
    Token make_token(TokenType type, const std::string& literal);
    Token error_token(const std::string& message);

    bool is_at_end();
    char advance();
    char peek();
    char peek_next();
    bool match(char expected);

    Token string_token();
    Token number_token();
    Token identifier_token();

    std::string source;
    size_t start = 0;
    size_t current = 0;
    size_t line_start = 0;
    int line = 1;
};

#endif // SAPPHIRE_LEXER_H
