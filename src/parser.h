#ifndef SAPPHIRE_PARSER_H
#define SAPPHIRE_PARSER_H

#include "ast.h"
#include <memory>
#include <vector>
#include <stdexcept>

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::vector<std::unique_ptr<Stmt>> parse();

private:
    std::vector<Token> tokens;
    int current = 0;

    // Métodos para cada regra da gramática
    std::unique_ptr<Stmt> declaration();
    std::unique_ptr<Stmt> var_declaration();
    std::unique_ptr<Stmt> statement();
    std::unique_ptr<Stmt> if_statement();
    std::unique_ptr<Stmt> print_statement();
    std::unique_ptr<Stmt> expression_statement();
    std::vector<std::unique_ptr<Stmt>> block();

    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> assignment();
    std::unique_ptr<Expr> logical_or();
    std::unique_ptr<Expr> logical_and();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> primary();

    // Funções de ajuda
    void synchronize();
    bool is_at_end();
    Token peek();
    Token previous();
    Token advance();
    bool check(TokenType type);
    bool match(const std::vector<TokenType>& types);
    Token consume(TokenType type, const std::string& message);
};

#endif // SAPPHIRE_PARSER_H