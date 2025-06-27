#ifndef SAPPHIRE_PARSER_H
#define SAPPHIRE_PARSER_H

#include "lexer.h"
#include "compiler.h"
#include <map>

class Parser {
public:
    Parser(Lexer& lexer, Compiler* compiler);
    void advance();
    void declaration();
    bool match(TokenType type);
    void emit_return();
    bool had_error;

private:
    Lexer& lexer;
    Compiler* current_compiler;
    Token current;
    Token previous;
    Token next;
    bool panic_mode;
    std::map<TokenType, ParseRule> rules;
    void function(TokenType kind, TokenType return_type);
    ObjFunction* end_compiler_scope();

    uint8_t argument_list(); 
    void error_at(const Token& token, const std::string& message);
    void error(const std::string& message);
    void error_at_current(const std::string& message);
    void consume(TokenType type, const std::string& message);
    bool check(TokenType type);
    bool check_next(TokenType type);
    Chunk* current_chunk();
    void emit_byte(uint8_t byte);
    void emit_bytes(uint8_t byte1, uint8_t byte2);
    uint8_t make_constant(const SapphireValue& value);
    void emit_constant(const SapphireValue& value);
    int emit_jump(uint8_t instruction);
    void patch_jump(int offset);
    TokenType parse_precedence(Precedence precedence);
    ParseRule* get_rule(TokenType type);
    void initialize_rules();
    TokenType expression();
    void statement();
    void block();
    void begin_scope();
    void end_scope();
    void print_statement();
    void expression_statement();
    void if_statement();
    void while_statement();
    void declaration_statement();
    void synchronize();
    TokenType grouping(bool can_assign);
    TokenType number(bool can_assign);
    TokenType string(bool can_assign);
    TokenType literal(bool can_assign);
    TokenType variable(bool can_assign);
    TokenType unary(bool can_assign);
    TokenType binary(TokenType left_type, bool can_assign); 
    TokenType call(TokenType left_type, bool can_assign);
    void class_declaration();
    TokenType dot(TokenType left_type, bool can_assign);
    uint8_t identifier_constant(const Token& name);
    void add_local(Token name, TokenType type);
    int resolve_local(Compiler* compiler, const Token& name);
    void declare_variable(const Token& name, TokenType type);
    uint8_t parse_variable(const std::string& error_message, TokenType type);
    void mark_initialized();
    void define_variable(uint8_t global);
    void emit_loop(int loop_start);
    void function_declaration();
    void function(TokenType kind);
    void return_statement();
    TokenType array_literal(bool can_assign);
    TokenType subscript(TokenType left_type, bool can_assign);
    TokenType this_expression(bool can_assign);
    void field_declaration();
};

#endif //SAPPHIRE_PARSER_H