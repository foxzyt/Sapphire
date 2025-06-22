#ifndef SAPPHIRE_LEXER_H
#define SAPPHIRE_LEXER_H
#include "tokens.h"
#include <vector>
#include <map>
#include <string>
class Lexer {
public: Lexer(std::string source); std::vector<Token> scan_tokens();
private: std::string source; std::vector<Token> tokens; int start=0; int current=0; bool is_at_end(); char advance(); void add_token(TokenType type); void add_token(TokenType type, const std::string& literal); bool match(char expected); char peek(); char peek_next(); void scan_token();
};
#endif