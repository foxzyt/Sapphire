#include "parser.h"
#include <iostream>
#include <utility>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

std::vector<std::unique_ptr<Stmt>> Parser::parse() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!is_at_end()) {
        auto stmt = declaration();
        if (stmt) statements.push_back(std::move(stmt));
    }
    return statements;
}

std::unique_ptr<Stmt> Parser::declaration() {
    try {
        if (match({TokenType::VAR})) return var_declaration();
        return statement();
    } catch (const std::runtime_error& e) {
        std::cerr << "Erro de Parse: " << e.what() << std::endl;
        synchronize();
        return nullptr;
    }
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) return;
        switch (peek().type) {
            case TokenType::VAR:
            case TokenType::PRINT:
            case TokenType::IF:
                return;
            default:
                break;
        }
        advance();
    }
}

std::unique_ptr<Stmt> Parser::statement() {
    if (match({TokenType::IF})) return if_statement();
    if (match({TokenType::PRINT})) return print_statement();
    if (match({TokenType::LEFT_BRACE})) {
        return std::make_unique<BlockStmt>(block());
    }
    return expression_statement();
}

std::unique_ptr<Stmt> Parser::if_statement() {
    consume(TokenType::LEFT_PAREN, "Esperava '(' depois de 'if'.");
    auto condition = expression();
    consume(TokenType::RIGHT_PAREN, "Esperava ')' depois da condicao.");
    auto then_branch = statement();
    std::unique_ptr<Stmt> else_branch = nullptr;
    if (match({TokenType::ELSE})) {
        else_branch = statement();
    }
    return std::make_unique<IfStmt>(std::move(condition), std::move(then_branch), std::move(else_branch));
}

std::vector<std::unique_ptr<Stmt>> Parser::block() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TokenType::RIGHT_BRACE) && !is_at_end()) {
        statements.push_back(declaration());
    }
    consume(TokenType::RIGHT_BRACE, "Esperava '}' no final do bloco.");
    return statements;
}

std::unique_ptr<Stmt> Parser::var_declaration() {
    Token name = consume(TokenType::IDENTIFIER, "Esperava um nome de variavel.");
    std::unique_ptr<Expr> initializer = nullptr;
    if (match({TokenType::EQUAL})) {
        initializer = expression();
    }
    consume(TokenType::SEMICOLON, "Esperava um ';' depois da declaracao da variavel.");
    return std::make_unique<VarStmt>(name, std::move(initializer));
}

std::unique_ptr<Stmt> Parser::print_statement() {
    auto value = expression();
    consume(TokenType::SEMICOLON, "Esperava um ';' depois do valor do print.");
    return std::make_unique<PrintStmt>(std::move(value));
}

std::unique_ptr<Stmt> Parser::expression_statement() {
    auto expr = expression();
    consume(TokenType::SEMICOLON, "Esperava um ';' depois da expressao.");
    return std::make_unique<ExpressionStmt>(std::move(expr));
}

// --- Hierarquia de Expressões com Precedência ---
std::unique_ptr<Expr> Parser::expression() { return assignment(); }

std::unique_ptr<Expr> Parser::assignment() {
    auto expr = logical_or();
    if (match({TokenType::EQUAL})) {
        Token equals = previous();
        auto value = assignment();
        if (auto* var = dynamic_cast<Variable*>(expr.get())) {
            return std::make_unique<AssignExpr>(var->name, std::move(value));
        }
        throw std::runtime_error("Alvo de atribuicao invalido.");
    }
    return expr;
}

std::unique_ptr<Expr> Parser::logical_or() {
    auto expr = logical_and();
    while (match({TokenType::OR})) {
        Token op = previous();
        auto right = logical_and();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::logical_and() {
    auto expr = equality();
    while (match({TokenType::AND})) {
        Token op = previous();
        auto right = equality();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::equality() {
    auto expr = comparison();
    while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
        Token op = previous();
        auto right = comparison();
        expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    auto expr = term();
    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL})) {
        Token op = previous();
        auto right = term();
        expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    auto expr = factor();
    while (match({TokenType::MINUS, TokenType::PLUS})) {
        Token op = previous();
        auto right = factor();
        expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::factor() {
    auto expr = unary();
    while (match({TokenType::SLASH, TokenType::STAR})) {
        Token op = previous();
        auto right = unary();
        expr = std::make_unique<Binary>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS})) {
        Token op = previous();
        auto right = unary();
        return std::make_unique<Unary>(op, std::move(right));
    }
    return primary();
}

std::unique_ptr<Expr> Parser::primary() {
    if (match({TokenType::TRUE})) return std::make_unique<Literal>(Token{TokenType::TRUE, "1.0"});
    if (match({TokenType::FALSE})) return std::make_unique<Literal>(Token{TokenType::FALSE, "0.0"});
    if (match({TokenType::NIL})) return std::make_unique<Literal>(Token{TokenType::NIL, "0.0"});
    if (match({TokenType::NUMBER})) {
        return std::make_unique<Literal>(previous());
    }
    if (match({TokenType::IDENTIFIER})) {
        return std::make_unique<Variable>(previous());
    }
    if (match({TokenType::LEFT_PAREN})) {
        auto expr = expression();
        consume(TokenType::RIGHT_PAREN, "Esperava um ')' depois da expressao.");
        return std::make_unique<Grouping>(std::move(expr));
    }
    throw std::runtime_error("Esperava uma expressao.");
}

Token Parser::consume(TokenType type, const std::string& message) { if (check(type)) return advance(); throw std::runtime_error(message); }
bool Parser::match(const std::vector<TokenType>& types) { for (TokenType type : types) { if (check(type)) { advance(); return true; } } return false; }
bool Parser::check(TokenType type) { if (is_at_end()) return false; return peek().type == type; }
Token Parser::advance() { if (!is_at_end()) current++; return previous(); }
bool Parser::is_at_end() { return peek().type == TokenType::END_OF_FILE; }
Token Parser::peek() { if (current >= tokens.size()) return tokens.back(); return tokens[current]; }
Token Parser::previous() { return tokens.at(current - 1); }