#ifndef SAPPHIRE_PARSER_H
#define SAPPHIRE_PARSER_H

#include <cstdint> // Para uint8_t, uint16_t
#include <string>  // Para std::string
#include <map>     // Para std::map
#include <functional> // Para std::function

#include "tokens.h"
#include "lexer.h"
#include "object.h" // Define ObjFunction e Chunk
#include "value.h"  // Define SapphireValue

class Compiler;
class VM;

enum Precedence {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
};

using PrefixParseFn = std::function<TokenType(bool)>;
using InfixParseFn = std::function<TokenType(TokenType, bool)>;

struct ParseRule {
    std::function<TokenType(bool)> prefix;
    std::function<TokenType(TokenType, bool)> infix;
    Precedence precedence;
};


class Parser {
public:
    Parser(Lexer& lexer, Compiler* compiler, VM* vm);
    void advance();
    void declaration();
    bool match(TokenType type);
    void emit_return();
    bool had_error;

private:
    Lexer& lexer;
    Compiler* current_compiler;
    VM* vm;
    Token current;
    Token previous;
    Token next;
    bool panic_mode;
    std::map<TokenType, ParseRule> rules;

    // Funções auxiliares
    void error_at(const Token& token, const std::string& message);
    void error(const std::string& message);
    void error_at_current(const std::string& message);
    void consume(TokenType type, const std::string& message);
    bool check(TokenType type);
    bool check_next(TokenType type);
    Chunk* current_chunk();
    void emit_byte(uint8_t byte);
    void emit_bytes(uint8_t byte1, uint8_t byte2);
    void emit_constant(const SapphireValue& value);
    int emit_jump(uint8_t instruction);
    void patch_jump(int offset);
    void emit_loop(int loop_start);

    // Funções de parsing
    TokenType parse_precedence(Precedence precedence);
    ParseRule* get_rule(TokenType type);
    void initialize_rules();
    TokenType expression();
    void statement();
    void expression_statement();
    void block();
    void synchronize();

    // Declarações
    void declaration_statement();
    void class_declaration();
    void function_declaration();
    ObjFunction* function(TokenType kind, TokenType return_type);
    void return_statement();
    void field_declaration();
    void import_statement();

    // Escopo
    void begin_scope();
    void end_scope();
    ObjFunction* end_compiler_scope();
    void print_statement();
    void if_statement();
    void while_statement();

    // Variáveis
    uint16_t identifier_constant(const Token& name);
    void add_local(Token name, TokenType type);
    int resolve_local(Compiler* compiler, const Token& name);
    void declare_variable(const Token& name, TokenType type);
    uint16_t parse_variable(const std::string& error_message, TokenType type);
    void mark_initialized();
    void define_variable(uint16_t global);

    // Expressões
    TokenType grouping(bool can_assign);
    TokenType number(bool can_assign);
    TokenType string(bool can_assign);
    TokenType literal(bool can_assign);
    TokenType variable(bool can_assign);
    TokenType unary(bool can_assign);
    TokenType binary(TokenType left_type, bool can_assign);
    TokenType call(TokenType left_type, bool can_assign);
    TokenType dot(TokenType left_type, bool can_assign);
    TokenType array_literal(bool can_assign);
    TokenType subscript(TokenType left_type, bool can_assign);
    TokenType this_expression(bool can_assign);
    TokenType and_(TokenType left_type, bool can_assign);
    TokenType or_(TokenType left_type, bool can_assign);
    uint8_t argument_list();
};

#endif //SAPPHIRE_PARSER_H