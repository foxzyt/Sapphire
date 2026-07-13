#include "parser.h"
#include "opcodes.h"
#include "debug.h"
#include "vm.h"
#include "tokens.h"
#include "compiler.h"
#include "utils.h"
#include "termcolor.h"
#include <iostream>
#include <stdexcept>

using enum TokenType;

// Função auxiliar movida para fora, pois não precisa ser membro
static bool types_are_compatible(TokenType variable_type, TokenType value_type) {
    //std::cout << "[DEBUG_TYPE_CHECK] Comparando Variavel (tipo " << (int)variable_type
    //          << ") com Valor (tipo " << (int)value_type << ")" << std::endl;

    if (value_type == TokenType::TOKEN_ILLEGAL) return true;
    if (variable_type == value_type) return true;
    if (variable_type == TokenType::TOKEN_VOID && value_type == TokenType::TOKEN_NIL) return true;

    // --- CORREÇÃO FINAL COM static_cast --- OBS: não era final
    // Convertemos os números para o tipo TokenType antes de comparar
    bool is_class_type = (variable_type == static_cast<TokenType>(39) || variable_type == static_cast<TokenType>(35));
    bool is_nil_value = (value_type == static_cast<TokenType>(26));

    if (is_class_type && is_nil_value) {
        // std::cout << "[DEBUG_TYPE_CHECK] Regra 'objeto = nil' ativada. Permitindo atribuicao." << std::endl;
        // novamente, vai que eu preciso
        return true;
    }

    bool var_is_numeric = (variable_type == TokenType::TOKEN_INT || variable_type == TokenType::TOKEN_DOUBLE || variable_type == TokenType::TOKEN_FLOAT);
    bool val_is_numeric = (value_type == TokenType::TOKEN_INT || value_type == TokenType::TOKEN_DOUBLE || value_type == TokenType::TOKEN_FLOAT);

    return var_is_numeric && val_is_numeric;
}


// Construtor Corrigido para aceitar a VM
Parser::Parser(Lexer& lexer, Compiler* compiler, VM* vm)
    : lexer(lexer), current_compiler(compiler), vm(vm) {
    had_error = false;
    panic_mode = false;
    advance();
    advance();
    initialize_rules();
}

// Funções de erro
void Parser::error_at(const Token& token, const std::string& message) {
    if (panic_mode) return;
    panic_mode = true;
    had_error = true;

    std::cerr << "\n" << tc_red() << "[Line " << token.line << ":" << token.column << "] Error";
    if (token.type == TokenType::TOKEN_END_OF_FILE) {
        std::cerr << " at end";
    } else {
        std::cerr << " at '" << token.literal << "'";
    }
    std::cerr << ":" << tc_reset() << " " << message << "\n\n";

    // Extract the exact line from source
    const std::string& source = lexer.get_source();
    size_t line_start = 0;
    size_t line_end = 0;
    int current_line = 1;

    for (size_t i = 0; i <= source.length(); i++) {
        if (i == source.length() || source[i] == '\n') {
            if (current_line == token.line) {
                line_end = i;
                if (line_end > 0 && source[line_end - 1] == '\r') line_end--;
                break;
            }
            line_start = i + 1;
            current_line++;
        }
    }

    if (line_end >= line_start) {
        std::string line_text = source.substr(line_start, line_end - line_start);
        
        // Replace tabs with spaces for consistent column alignment
        for (char& c : line_text) { if (c == '\t') c = ' '; }

        std::cerr << " " << std::setw(4) << token.line << " | " << line_text << "\n";
        std::cerr << "      | ";
        
        int pad = std::max(0, token.column - 1);
        for (int i = 0; i < pad; i++) std::cerr << " ";
        
        std::cerr << tc_red() << "^";
        int len = std::max(1, token.length);
        for (int i = 1; i < len; i++) std::cerr << "~";
        std::cerr << tc_reset() << "\n\n";
    }
}
void Parser::error(const std::string& message) { error_at(previous, message); }
void Parser::error_at_current(const std::string& message) { error_at(current, message); }

// Funções de controle de tokens
void Parser::advance() {
    previous = current;
    current = next;
    // std::cout << "[PARSER DEBUG] Advancing. New current token: " << token_type_to_string(current.type)
    //         << ", Literal: '" << current.literal << "', Line: " << current.line << std::endl;
    // AVISO : Não coloque isso, vai travar o script sapphire, eu acho..
    for (;;) {
        next = lexer.scan_token();
        if (next.type != TokenType::TOKEN_ILLEGAL) break;
        error_at(next, "Unexpected character: " + next.literal);
    }
}

void Parser::consume(TokenType type, const std::string& message) {
    if (current.type == type) {
        advance();
        return;
    }
    
    // Optional Semicolon logic
    if (type == TokenType::TOKEN_SEMICOLON) {
        if (previous.line < current.line || 
            current.type == TokenType::TOKEN_RIGHT_BRACE || 
            current.type == TokenType::TOKEN_END_OF_FILE) {
            return; // Semicolon inferred
        }
    }
    
    error_at_current(message);
}
bool Parser::check(TokenType type) { return current.type == type; }
bool Parser::check_next(TokenType type) { return next.type == type; }
bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

// Funções de emissão de bytecode
Chunk* Parser::current_chunk() { return &current_compiler->function->chunk; }
void Parser::emit_byte(uint8_t byte) { current_chunk()->write(byte); }
void Parser::emit_bytes(uint8_t byte1, uint8_t byte2) { emit_byte(byte1); emit_byte(byte2); }
void Parser::emit_return() { emit_byte(OP_NIL); emit_byte(OP_RETURN); }

void Parser::emit_constant(const SapphireValue& value) {
    int index = current_chunk()->add_constant(value);
    // Lógica de 16-bit para constantes
    // Próximo é 32-bit, pois é fácil de implementar, só mudar o UINT16 para UINT32, eu acho..
    if (index > UINT16_MAX) {
        error("Too many constants in one chunk.");
        index = 0;
    }
    emit_byte(OP_CONSTANT);
    emit_byte((index >> 8) & 0xFF);
    emit_byte(index & 0xFF);
}

int Parser::emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->code.size() - 2;
}

void Parser::patch_jump(int offset) {
    int jump = current_chunk()->code.size() - offset - 2;
    if (jump > UINT16_MAX) {
        error("Jump is too long to be encoded."); // eu não sei o que fazer para corrigir essa função
    }
    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

void Parser::emit_loop(int loop_start) {
    emit_byte(OP_LOOP);
    int offset = current_chunk()->code.size() - loop_start + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }
    // Emite o offset como dois bytes (big-endian)
    // big endian : indiano grande (sem ofensas)
    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

uint8_t Parser::argument_list() {
    uint8_t arg_count = 0;
    if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
        do {
            if (check(TokenType::TOKEN_IDENTIFIER) && check_next(TokenType::TOKEN_EQUAL)) {
                Token name = current;
                advance(); // consume identifier
                advance(); // consume '='
                emit_constant(new_string(vm, name.literal));
                expression();
                emit_byte(OP_MAKE_NAMED_ARG);
            } else {
                expression();
            }
            if (arg_count == 255) {
                error("Can't have more than 255 arguments.");
            }
            arg_count++;
        } while (match(TokenType::TOKEN_COMMA));
    }
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}

// Lógica principal do Parser
// Levei 7 horas para escrever isso, ou mais..
TokenType Parser::parse_precedence(Precedence precedence) {
    advance();
    PrefixParseFn prefix_rule = get_rule(previous.type)->prefix;
    if (!prefix_rule) {
        error("Expected expression.");
        return TokenType::TOKEN_ILLEGAL;
    }
    bool can_assign = precedence <= PREC_ASSIGNMENT;
    TokenType left_type = prefix_rule(can_assign);
    while (precedence <= get_rule(current.type)->precedence) {
        advance();
        InfixParseFn infix_rule = get_rule(previous.type)->infix;
        left_type = infix_rule(left_type, can_assign);
    }
    if (can_assign && match(TokenType::TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
    return left_type;
}
ParseRule* Parser::get_rule(TokenType type) {
    if (rules.count(type)) return &rules[type];
    return &rules[TokenType::TOKEN_ILLEGAL];
}

// Lógica de declaração de variáveis e escopo
uint16_t Parser::identifier_constant(const Token& name) {
    int index = current_chunk()->add_constant(new_string(vm, name.literal));
    if (index > UINT16_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint16_t)index;
}

void Parser::add_local(Token name, TokenType type, bool is_const) {
    if (current_compiler->local_count == 256) {
        error("Too many local variables in a function.");
        return;
    }
    Local* local = &current_compiler->locals[current_compiler->local_count++];
    local->name = name;
    local->depth = -1;
    local->type = type;
    local->is_const = is_const;
}

int Parser::resolve_local(Compiler* compiler, const Token& name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (name.literal == local->name.literal) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static TokenType resolve_global_type(Compiler* compiler, const std::string& name) {
    while (compiler != nullptr) {
        auto it = compiler->global_types.find(name);
        if (it != compiler->global_types.end()) {
            return it->second;
        }
        compiler = compiler->enclosing;
    }
    return TokenType::TOKEN_ILLEGAL;
} // merda, deu errado

void Parser::declare_variable(const Token& name, TokenType type, bool is_const) {
    if (current_compiler->scope_depth == 0) {
        current_compiler->global_types[name.literal] = type;
        current_compiler->global_consts[name.literal] = is_const;
        return;
    }
    for (int i = current_compiler->local_count - 1; i >= 0; i--) {
        Local* local = &current_compiler->locals[i];
        if (local->depth != -1 && local->depth < current_compiler->scope_depth) break;
        if (name.literal == local->name.literal) {
            // error("Already a variable with this name in this scope.");
        }
    }
    add_local(name, type, is_const);
}

uint16_t Parser::parse_variable(const std::string& error_message, TokenType type, bool is_const) {
    consume(TokenType::TOKEN_IDENTIFIER, error_message);
    declare_variable(previous, type, is_const);
    if (current_compiler->scope_depth > 0) return 0;
    return identifier_constant(previous);
} // ele vai levar a variável para passear, hehe

void Parser::mark_initialized() {
    if (current_compiler->scope_depth == 0) return;
    current_compiler->locals[current_compiler->local_count - 1].depth = current_compiler->scope_depth;
}

void Parser::define_variable(uint16_t global) {
    if (current_compiler->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit_byte(OP_DEFINE_GLOBAL);
    emit_byte((global >> 8) & 0xFF);
    emit_byte(global & 0xFF);
}

// eu perdi a noção do tempo, já é 11:00 da noite?

void Parser::begin_scope() { current_compiler->scope_depth++; }
void Parser::end_scope() {
    current_compiler->scope_depth--;
    while (current_compiler->local_count > 0 && current_compiler->locals[current_compiler->local_count - 1].depth > current_compiler->scope_depth) {
        emit_byte(OP_POP);
        current_compiler->local_count--;
    }
}

ObjFunction* Parser::end_compiler_scope() {
    emit_return();
    ObjFunction* function = current_compiler->function;
    #ifdef DEBUG_PRINT_CODE
        if (!had_error) {
            disassemble_chunk(function->chunk, function->name != nullptr ? function->name->chars : "<script>");
        }
        // porque que o código está meio transparente?
    #endif
    current_compiler = current_compiler->enclosing;
    return function;
}

// Parsing de statements e declarações
TokenType Parser::expression() { return parse_precedence(PREC_ASSIGNMENT); }

void Parser::block() {
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !check(TokenType::TOKEN_END_OF_FILE)) {
        declaration();
    }
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

void Parser::print_statement() {
    expression();
    consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

void Parser::if_statement() {
    consume(TokenType::TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    int else_jump = emit_jump(OP_JUMP);
    patch_jump(then_jump);
    emit_byte(OP_POP);
    if (match(TokenType::TOKEN_ELSE)) statement();
    patch_jump(else_jump);
    // ahn?
}

void Parser::while_statement() {
    int loop_start = current_chunk()->code.size();
    consume(TokenType::TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);
    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

void Parser::for_statement() {
    begin_scope();
    consume(TokenType::TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    
    // Initializer
    if (match(TokenType::TOKEN_SEMICOLON)) {
        // No initializer
    } else if (check(TokenType::TOKEN_CONST) || check(TokenType::TOKEN_VAR) || check(TokenType::TOKEN_INT) || check(TokenType::TOKEN_FLOAT) || check(TokenType::TOKEN_DOUBLE) || check(TokenType::TOKEN_BOOL) || check(TokenType::TOKEN_STRING)) {
        declaration_statement();
    } else {
        expression_statement();
    }
    
    int loop_start = current_chunk()->code.size();
    
    // Condition
    int exit_jump = -1;
    if (!match(TokenType::TOKEN_SEMICOLON)) {
        expression();
        consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }
    
    // Increment
    if (!match(TokenType::TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->code.size();
        
        expression();
        emit_byte(OP_POP);
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
        
        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }
    
    statement();
    emit_loop(loop_start);
    
    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }
    
    end_scope();
}

void Parser::enum_declaration() {
    consume(TokenType::TOKEN_IDENTIFIER, "Expect enum name.");
    Token enum_name = previous;
    uint16_t name_constant = identifier_constant(enum_name);
    declare_variable(enum_name, TokenType::TOKEN_CONST, false);
    
    ObjClass* enum_class = new_class(vm, new_string(vm, enum_name.literal));
    ObjInstance* instance = new_instance(vm, enum_class);
    
    consume(TokenType::TOKEN_LEFT_BRACE, "Expect '{' before enum body.");
    
    double enum_value = 0.0;
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !check(TokenType::TOKEN_END_OF_FILE)) {
        consume(TokenType::TOKEN_IDENTIFIER, "Expect enum value identifier.");
        Token value_name = previous;
        
        instance->fields[value_name.literal] = enum_value;
        enum_value += 1.0;
        
        if (!match(TokenType::TOKEN_COMMA)) {
            break;
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expect '}' after enum body.");
    
    emit_constant(instance);
    define_variable(name_constant);
}

void Parser::expression_statement() {
    expression();
    consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

void Parser::synchronize() {
    panic_mode = false;
    while (current.type != TokenType::TOKEN_END_OF_FILE) {
        if (previous.type == TokenType::TOKEN_SEMICOLON) return;
        switch (current.type) {
            case TokenType::TOKEN_CLASS:
            case TokenType::TOKEN_FUNCTION:
            case TokenType::TOKEN_INT:
            case TokenType::TOKEN_BOOL:
            case TokenType::TOKEN_STRING: // OBS: na linha 118 eu tinha escrito CINCO letras erradas e me fez dar aquele erro, agora que eu descobri eu me sinto um pateta
            case TokenType::TOKEN_DOUBLE:
            case TokenType::TOKEN_FLOAT:
            case TokenType::TOKEN_VOID:
            case TokenType::TOKEN_IF:
            case TokenType::TOKEN_WHILE:
            case TokenType::TOKEN_PRINT:
            case TokenType::TOKEN_RETURN:
                return;
            default:
                ; // Não faz nada
        } // então porque botou miséria?
        advance();
    }
}

void Parser::import_statement() {
    consume(TokenType::TOKEN_STRING_LITERAL, "Expect a string with the module path after 'import'.");

    uint16_t const_index = identifier_constant(previous);

    consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after module path.");

    emit_byte(static_cast<uint8_t>(OP_IMPORT));
    emit_byte((const_index >> 8) & 0xFF);
    emit_byte(const_index & 0xFF);

    emit_byte(OP_POP);
}




void Parser::declaration() {
    if (match(TokenType::TOKEN_CLASS)) {
        Token class_or_var_name;

        if (check(TokenType::TOKEN_IDENTIFIER)) {
            if (check_next(TokenType::TOKEN_LEFT_BRACE)) {
                class_declaration();
            } else {
                advance();
                Token class_or_var_name = previous;
                declare_variable(class_or_var_name, TokenType::TOKEN_CLASS, false);
                if (match(TokenType::TOKEN_EQUAL)) {
                    expression();
                } else {
                    emit_byte(OP_NIL);
                }
                consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after class variable declaration.");
                define_variable(identifier_constant(class_or_var_name));
            }
        } else {
            error_at_current("Expect identifier after 'class' keyword.");
        }
    }
    else if (match(TokenType::TOKEN_ENUM)) {
        enum_declaration();
    }
    else if (match(TokenType::TOKEN_IMPORT)) {
        import_statement();
    } else if (match(TokenType::TOKEN_FUNCTION)) {
        function_declaration();
    } else if (check(TokenType::TOKEN_INT) || check(TokenType::TOKEN_BOOL) || check(TokenType::TOKEN_STRING) ||
               check(TokenType::TOKEN_DOUBLE) || check(TokenType::TOKEN_FLOAT) || check(TokenType::TOKEN_VOID) ||
               check(TokenType::TOKEN_CONST) || check(TokenType::TOKEN_VAR) ||
               (check(TokenType::TOKEN_IDENTIFIER) && check_next(TokenType::TOKEN_IDENTIFIER))) { // Added check for ClassName variableName
        declaration_statement();
    } else {
        statement();
    }

    if (panic_mode) {
        synchronize();
    }
}

void Parser::statement() {
    if (match(TokenType::TOKEN_PRINT)) {
        print_statement();
    } else if (match(TokenType::TOKEN_RETURN)) {
        return_statement();
    } else if (match(TokenType::TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (match(TokenType::TOKEN_IF)) {
        if_statement();
    } else if (match(TokenType::TOKEN_WHILE)) {
        while_statement();
    } else if (match(TokenType::TOKEN_FOR)) {
        for_statement();
    } else {
        expression_statement();
    }
}

void Parser::declaration_statement() {
    bool is_const = false;
    bool is_inferred = false;

    if (match(TokenType::TOKEN_CONST)) {
        is_const = true;
        if (check(TokenType::TOKEN_IDENTIFIER)) {
            is_inferred = true;
        }
    } else if (match(TokenType::TOKEN_VAR)) {
        if (check(TokenType::TOKEN_IDENTIFIER)) {
            is_inferred = true;
        }
    }

    TokenType var_type = TokenType::TOKEN_ILLEGAL;

    if (!is_inferred) {
        var_type = current.type;
        advance();

        if (match(TokenType::TOKEN_LEFT_BRACKET)) {
            consume(TokenType::TOKEN_RIGHT_BRACKET, "Expect ']' after '[' in array type specifier.");
        }

        if (var_type == TokenType::TOKEN_IDENTIFIER) {
            var_type = TokenType::TOKEN_CLASS;
        }
    }

    consume(TokenType::TOKEN_IDENTIFIER, "Expected variable name.");
    Token var_name = previous;

    declare_variable(var_name, var_type, is_const);

    if (match(TokenType::TOKEN_EQUAL)) {
        TokenType expr_type = expression();

        if (is_inferred) {
            if (current_compiler->scope_depth == 0) {
                current_compiler->global_types[var_name.literal] = expr_type;
            } else {
                for (int i = current_compiler->local_count - 1; i >= 0; i--) {
                    if (current_compiler->locals[i].name.literal == var_name.literal) {
                        current_compiler->locals[i].type = expr_type;
                        break;
                    }
                }
            }
        }
    } else {
        if (is_const || is_inferred) {
            error("Const and inferred var declarations must be initialized.");
        }
        emit_byte(OP_NIL);
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

    if (current_compiler->scope_depth > 0) {
        mark_initialized();
    } else {
        uint16_t global_idx = identifier_constant(var_name);
        emit_byte(OP_DEFINE_GLOBAL);
        emit_byte((global_idx >> 8) & 0xFF);
        emit_byte(global_idx & 0xFF);
    }
}
// correção número #29
void Parser::function_declaration() {
    TokenType return_type = TokenType::TOKEN_VOID;

    uint16_t global = parse_variable("Expect function name.", TokenType::TOKEN_FUNCTION, false);

    ObjFunction* func = function(TokenType::TOKEN_FUNCTION, return_type);

    int index = current_chunk()->add_constant(func);
    if (index > UINT16_MAX) {
        error("Too many constants in one chunk.");
        index = 0;
    }

    emit_byte(OP_CLOSURE);
    emit_byte((index >> 8) & 0xFF);
    emit_byte(index & 0xFF);

    define_variable(global);
}

void Parser::class_declaration() {
    consume(TokenType::TOKEN_IDENTIFIER, "Expect class name.");
    Token class_name = previous;
    uint16_t name_constant = identifier_constant(class_name);
    declare_variable(class_name, TokenType::TOKEN_CLASS, false);

    ObjClass* klass = new_class(vm, new_string(vm, class_name.literal));

    consume(TokenType::TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !check(TokenType::TOKEN_END_OF_FILE)) {
        if (check(TokenType::TOKEN_FUNCTION)) {
            advance(); // Consome o 'function'. Consome haha

            TokenType method_default_return_type = TokenType::TOKEN_VOID;

            consume(TokenType::TOKEN_IDENTIFIER, "Expect method name.");
            Token method_name = previous;

            ObjFunction* method_body = function(TokenType::TOKEN_FUNCTION, method_default_return_type);
            klass->methods[method_name.literal] = SapphireValue((Obj*)new_closure(vm, method_body));

        } else if (check(TokenType::TOKEN_INT) || check(TokenType::TOKEN_BOOL) || check(TokenType::TOKEN_STRING) ||
                   check(TokenType::TOKEN_DOUBLE) || check(TokenType::TOKEN_FLOAT) || check(TokenType::TOKEN_VOID) ||
                   (check(TokenType::TOKEN_IDENTIFIER) && check_next(TokenType::TOKEN_IDENTIFIER))) {

            field_declaration();
        } else {
            error_at_current("Expect a method or field declaration inside class body.");
            while (!check(TokenType::TOKEN_SEMICOLON) && !check(TokenType::TOKEN_RIGHT_BRACE) && !check(TokenType::TOKEN_END_OF_FILE)) {
                advance();
            }
            if (match(TokenType::TOKEN_SEMICOLON)) {}
        }
    }


    consume(TokenType::TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

    emit_constant(klass);
    define_variable(name_constant);
}


ObjFunction* Parser::function(TokenType kind, TokenType default_return_type_if_not_explicit) {
    Compiler compiler(new_function(vm));
    compiler.enclosing = current_compiler;

    compiler.function_return_type = default_return_type_if_not_explicit;
    current_compiler = &compiler;
    current_compiler->function->name = new_string(vm, previous.literal);

    begin_scope();
    consume(TokenType::TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
        do {
            current_compiler->function->arity++;
            if (current_compiler->function->arity > 255) error_at_current("Can't have more than 255 parameters.");

            TokenType param_type = TokenType::TOKEN_ILLEGAL;

            if (match(TokenType::TOKEN_INT) || match(TokenType::TOKEN_BOOL) || match(TokenType::TOKEN_STRING) ||
                match(TokenType::TOKEN_DOUBLE) || match(TokenType::TOKEN_FLOAT) || match(TokenType::TOKEN_VOID) ||
                match(TokenType::TOKEN_CLASS)) {
                param_type = previous.type;
            }
            else if (check(TokenType::TOKEN_IDENTIFIER)) {
                advance();
                param_type = TokenType::TOKEN_CLASS;
            } else {
                error_at_current("Expect parameter type.");
                while (!check(TokenType::TOKEN_COMMA) && !check(TokenType::TOKEN_RIGHT_PAREN) && !check(TokenType::TOKEN_END_OF_FILE)) {
                     advance();
                }
                continue;
            }

            uint16_t param_const = parse_variable("Expect parameter name.", param_type, false);
            define_variable(param_const);
        } while (match(TokenType::TOKEN_COMMA));
    }
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    if (check(TokenType::TOKEN_INT) || check(TokenType::TOKEN_BOOL) || check(TokenType::TOKEN_STRING) ||
        check(TokenType::TOKEN_DOUBLE) || check(TokenType::TOKEN_FLOAT) || check(TokenType::TOKEN_VOID)) {
        advance();
        current_compiler->function_return_type = previous.type;
    }


    consume(TokenType::TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    return end_compiler_scope();
}

void Parser::return_statement() {
    if (current_compiler->function_return_type == TokenType::TOKEN_VOID) {
        if (previous.line == current.line && current.type != TokenType::TOKEN_SEMICOLON && current.type != TokenType::TOKEN_RIGHT_BRACE) {
            TokenType value_type = expression();
            error("A 'void' function cannot return a value.");
            emit_byte(OP_POP);
        }
        consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after return statement.");
        emit_return();
    } else {
        if (previous.line < current.line || current.type == TokenType::TOKEN_SEMICOLON || current.type == TokenType::TOKEN_RIGHT_BRACE) {
            error("A non-void function must return a value.");
            emit_return();
        } else {
            TokenType value_type = expression();
            if (current_compiler->enclosing != nullptr) {
                if (!types_are_compatible(current_compiler->function_return_type, value_type)) {
                    error("Return value type does not match function return type.");
                }
            }
            emit_byte(OP_RETURN);
        }
        consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after return statement.");
    }
}
void Parser::field_declaration() {
    advance();
    consume(TokenType::TOKEN_IDENTIFIER, "Expect field name.");
    consume(TokenType::TOKEN_SEMICOLON, "Expect ';' after field declaration.");
}


TokenType Parser::grouping(bool can_assign) {
    TokenType type = expression();
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    return type;
}

TokenType Parser::number(bool can_assign) {
    try {
        double value = std::stod(previous.literal);
        emit_constant(value);
    } catch (const std::out_of_range&) {
        error("Numeric value out of bounds.");
        emit_constant(0.0);
    }
    return TokenType::TOKEN_DOUBLE;
}

TokenType Parser::string(bool can_assign) {
    emit_constant(new_string(vm, previous.literal));
    return TokenType::TOKEN_STRING;
}

TokenType Parser::call(TokenType left_type, bool can_assign) {
    uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);

    if (left_type == TokenType::TOKEN_CLASS) {
        return TokenType::TOKEN_CLASS;
    }
    return TokenType::TOKEN_ILLEGAL;
}

TokenType Parser::unary(bool can_assign) {
    TokenType operator_type = previous.type;
    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TokenType::TOKEN_MINUS: emit_byte(OP_NEGATE); return TokenType::TOKEN_DOUBLE;
        case TokenType::TOKEN_BANG:  emit_byte(OP_NOT); return TokenType::TOKEN_BOOL;
        default: return TokenType::TOKEN_ILLEGAL;
    }
}

static TokenType resolve_local_type(Compiler* compiler, const Token& name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (name.literal == local->name.literal) {
            return local->type;
        }
    }
    return TokenType::TOKEN_ILLEGAL;
}

TokenType Parser::binary(TokenType left_type, bool can_assign) {
    TokenType operator_type = previous.type;
    ParseRule* rule = get_rule(operator_type);
    TokenType right_type = parse_precedence((Precedence)(rule->precedence + 1));

    bool left_is_numeric = (left_type == TokenType::TOKEN_INT || left_type == TokenType::TOKEN_DOUBLE || left_type == TokenType::TOKEN_FLOAT);
    bool right_is_numeric = (right_type == TokenType::TOKEN_INT || right_type == TokenType::TOKEN_DOUBLE || right_type == TokenType::TOKEN_FLOAT);

    switch (operator_type) {
      case TokenType::TOKEN_PLUS:
    if (left_type != TokenType::TOKEN_ILLEGAL && right_type != TokenType::TOKEN_ILLEGAL) {
        bool valid = (left_is_numeric && right_is_numeric) ||
                     (left_type == TokenType::TOKEN_STRING && right_type == TokenType::TOKEN_STRING) ||
                     (left_type == TokenType::TOKEN_STRING && right_is_numeric) ||
                     (left_is_numeric && right_type == TokenType::TOKEN_STRING) ||
                     (left_type == TokenType::TOKEN_CLASS || right_type == TokenType::TOKEN_CLASS);
        if (!valid && !this->vm->soft_mode) {
             error("The '+' operator requires two numbers or two strings.");
        }
    }
    emit_byte(OP_ADD);
    break;
      case TokenType::TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
      case TokenType::TOKEN_STAR:  emit_byte(OP_MULTIPLY); break;
      case TokenType::TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
      case TokenType::TOKEN_PERCENT: emit_byte(OP_MODULO); break;
      case TokenType::TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
      case TokenType::TOKEN_BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT); break;
      case TokenType::TOKEN_GREATER:       emit_byte(OP_GREATER); break;
      case TokenType::TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
      case TokenType::TOKEN_LESS:          emit_byte(OP_LESS); break;
      case TokenType::TOKEN_LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
      default:
        return TokenType::TOKEN_ILLEGAL;
    }

    if (left_is_numeric && right_is_numeric) {
        if (operator_type >= TokenType::TOKEN_EQUAL_EQUAL) return TokenType::TOKEN_BOOL;
        if (left_type == TokenType::TOKEN_DOUBLE || right_type == TokenType::TOKEN_DOUBLE) return TokenType::TOKEN_DOUBLE;
        return TokenType::TOKEN_INT;
    }
    if ((left_type == TokenType::TOKEN_STRING || right_type == TokenType::TOKEN_STRING) && operator_type == TokenType::TOKEN_PLUS) {
        return TokenType::TOKEN_STRING;
    }
    if (left_type == TokenType::TOKEN_STRING && right_type == TokenType::TOKEN_STRING) {
        if (operator_type == TokenType::TOKEN_EQUAL_EQUAL || operator_type == TokenType::TOKEN_BANG_EQUAL) return TokenType::TOKEN_BOOL;
    }
    return TokenType::TOKEN_ILLEGAL;
}

TokenType Parser::literal(bool can_assign) {
    switch (previous.type) {
        case TokenType::TOKEN_FALSE: emit_byte(OP_FALSE); return TokenType::TOKEN_BOOL;
        case TokenType::TOKEN_TRUE:  emit_byte(OP_TRUE);  return TokenType::TOKEN_BOOL;
        case TokenType::TOKEN_NIL:   emit_byte(OP_NIL);   return TokenType::TOKEN_NIL;
        default: return TokenType::TOKEN_ILLEGAL;
    }
}

// indentação, o que é indentação?

TokenType Parser::variable(bool can_assign) {
    Token name = previous;
    int arg;
    uint8_t get_op, set_op;
    TokenType var_type;

    bool is_const = false;

    arg = resolve_local(current_compiler, name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL; set_op = OP_SET_LOCAL;
        var_type = resolve_local_type(current_compiler, name);
        is_const = current_compiler->locals[arg].is_const;
    } else {
        arg = identifier_constant(name);
        get_op = OP_GET_GLOBAL; set_op = OP_SET_GLOBAL;
        var_type = resolve_global_type(current_compiler, name.literal);
        is_const = current_compiler->global_consts[name.literal];
    }

    if (can_assign) {
        bool is_compound = false;
        bool is_assignment = false;
        uint8_t compound_op = OP_NIL;

        if (match(TokenType::TOKEN_EQUAL)) {
            is_assignment = true;
        } else if (match(TokenType::TOKEN_PLUS_EQUAL)) { is_compound = true; compound_op = OP_ADD; }
        else if (match(TokenType::TOKEN_MINUS_EQUAL)) { is_compound = true; compound_op = OP_SUBTRACT; }
        else if (match(TokenType::TOKEN_STAR_EQUAL)) { is_compound = true; compound_op = OP_MULTIPLY; }
        else if (match(TokenType::TOKEN_SLASH_EQUAL)) { is_compound = true; compound_op = OP_DIVIDE; }
        else if (match(TokenType::TOKEN_PLUS_PLUS)) {
            if (is_const) error("Cannot reassign a constant variable.");
            if (get_op == OP_GET_LOCAL) { emit_bytes(get_op, (uint8_t)arg); } else { emit_byte(get_op); emit_byte((arg >> 8) & 0xFF); emit_byte(arg & 0xFF); }
            emit_constant(1.0);
            emit_byte(OP_ADD);
            if (set_op == OP_SET_LOCAL) { emit_bytes(set_op, (uint8_t)arg); } else { emit_byte(set_op); emit_byte((arg >> 8) & 0xFF); emit_byte(arg & 0xFF); }
            return var_type;
        }
        else if (match(TokenType::TOKEN_MINUS_MINUS)) {
            if (is_const) error("Cannot reassign a constant variable.");
            if (get_op == OP_GET_LOCAL) { emit_bytes(get_op, (uint8_t)arg); } else { emit_byte(get_op); emit_byte((arg >> 8) & 0xFF); emit_byte(arg & 0xFF); }
            emit_constant(1.0);
            emit_byte(OP_SUBTRACT);
            if (set_op == OP_SET_LOCAL) { emit_bytes(set_op, (uint8_t)arg); } else { emit_byte(set_op); emit_byte((arg >> 8) & 0xFF); emit_byte(arg & 0xFF); }
            return var_type;
        }

        if (is_assignment || is_compound) {
            if (is_const) error("Cannot reassign a constant variable.");

            if (is_compound) {
                if (get_op == OP_GET_LOCAL) { emit_bytes(get_op, (uint8_t)arg); } else { emit_byte(get_op); emit_byte((arg >> 8) & 0xFF); emit_byte(arg & 0xFF); }
            }

            TokenType assigned_type = expression();

            if (is_compound) {
                emit_byte(compound_op);
                assigned_type = var_type;
            } else if (var_type != TokenType::TOKEN_ILLEGAL && !types_are_compatible(var_type, assigned_type)) {
                error("Incompatible types for assignment.");
            }

            if (set_op == OP_SET_LOCAL) {
                emit_bytes(set_op, (uint8_t)arg);
            } else {
                emit_byte(set_op);
                emit_byte((arg >> 8) & 0xFF);
                emit_byte(arg & 0xFF);
            }
            return assigned_type;
        }
    }

    if (get_op == OP_GET_LOCAL) {
        emit_bytes(get_op, (uint8_t)arg);
    } else {
        emit_byte(get_op);
        emit_byte((arg >> 8) & 0xFF);
        emit_byte(arg & 0xFF);
    }
    return var_type;
}

TokenType Parser::and_(TokenType left_type, bool can_assign) {
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    parse_precedence(PREC_AND);
    patch_jump(end_jump);
    return TokenType::TOKEN_BOOL;
}

TokenType Parser::or_(TokenType left_type, bool can_assign) {
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);
    patch_jump(else_jump);
    emit_byte(OP_POP);
    parse_precedence(PREC_OR);
    patch_jump(end_jump);
    return TokenType::TOKEN_BOOL;
}

void Parser::initialize_rules() {
    rules[TokenType::TOKEN_LEFT_PAREN]    = { [this](bool b){ return grouping(b); },      [this](TokenType l, bool b){ return call(l, b); }, PREC_CALL };
    rules[TokenType::TOKEN_DOT]           = { nullptr,                                    [this](TokenType l, bool b){ return dot(l, b); },    PREC_CALL };
    rules[TokenType::TOKEN_MINUS]         = { [this](bool b){ return unary(b); },         [this](TokenType l, bool b){ return binary(l, b); }, PREC_TERM };
    rules[TokenType::TOKEN_PLUS]          = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_TERM };
    rules[TokenType::TOKEN_SLASH]         = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_FACTOR };
    rules[TokenType::TOKEN_STAR]          = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_FACTOR };
    rules[TokenType::TOKEN_PERCENT]       = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_FACTOR };
    rules[TokenType::TOKEN_EQUAL_EQUAL]   = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_EQUALITY };
    rules[TokenType::TOKEN_BANG_EQUAL]    = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_EQUALITY };
    rules[TokenType::TOKEN_GREATER]       = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_COMPARISON };
    rules[TokenType::TOKEN_GREATER_EQUAL] = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_COMPARISON };
    rules[TokenType::TOKEN_LESS]          = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_COMPARISON };
    rules[TokenType::TOKEN_LESS_EQUAL]    = { nullptr,                                    [this](TokenType l, bool b){ return binary(l, b); }, PREC_COMPARISON };
    rules[TokenType::TOKEN_AND]           = { nullptr,                                    [this](TokenType l, bool b){ return and_(l, b); },  PREC_AND };
    rules[TokenType::TOKEN_OR]            = { nullptr,                                    [this](TokenType l, bool b){ return or_(l, b); },   PREC_OR };
    rules[TokenType::TOKEN_BANG]          = { [this](bool b){ return unary(b); },         nullptr, PREC_NONE };
    rules[TokenType::TOKEN_IDENTIFIER]    = { [this](bool b){ return variable(b); },      nullptr, PREC_NONE };
    rules[TokenType::TOKEN_NUMBER]        = { [this](bool b){ return number(b); },         nullptr, PREC_NONE };
    rules[TokenType::TOKEN_STRING_LITERAL]= { [this](bool b){ return string(b); },         nullptr, PREC_NONE };
    rules[TokenType::TOKEN_FALSE]         = { [this](bool b){ return literal(b); },       nullptr, PREC_NONE };
    rules[TokenType::TOKEN_TRUE]          = { [this](bool b){ return literal(b); },        nullptr, PREC_NONE };
    rules[TokenType::TOKEN_NIL]           = { [this](bool b){ return literal(b); },        nullptr, PREC_NONE };
    rules[TokenType::TOKEN_LEFT_BRACKET]  = { [this](bool b){ return array_literal(b); }, [this](TokenType l, bool b){ return subscript(l, b); }, PREC_CALL };
    rules[TokenType::TOKEN_THIS]          = { [this](bool b){ return this_expression(b); },nullptr, PREC_NONE };
    rules[TokenType::TOKEN_ILLEGAL]       = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::TOKEN_RIGHT_PAREN]   = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::TOKEN_LEFT_BRACE]    = { [this](bool b){ return map_literal(b); }, nullptr, PREC_NONE };
    rules[TokenType::TOKEN_RIGHT_BRACE]   = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::TOKEN_COMMA]         = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::TOKEN_EQUAL]         = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::TOKEN_SEMICOLON]     = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::TOKEN_AND]           = { nullptr, [this](TokenType l, bool b){ return and_(l, b); },  PREC_AND };
} // eu levei 30 minutos para escrever essa função e tive que trocar todo token para colocar TOKEN_ no início.

TokenType Parser::dot(TokenType left_type, bool can_assign) {
    consume(TokenType::TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint16_t name = identifier_constant(previous);
    if (can_assign && match(TokenType::TOKEN_EQUAL)) {
        expression();
        emit_byte(OP_SET_PROPERTY);
        emit_byte((name >> 8) & 0xFF);
        emit_byte(name & 0xFF);
    } else {
        emit_byte(OP_GET_PROPERTY);
        emit_byte((name >> 8) & 0xFF);
        emit_byte(name & 0xFF);
    }
    return TokenType::TOKEN_ILLEGAL;
}

TokenType Parser::array_literal(bool can_assign) {
    uint8_t element_count = 0;
    if (!check(TokenType::TOKEN_RIGHT_BRACKET)) {
        do {
            expression();
            if (element_count == 255) {
                error("Cannot have more than 255 elements in an array literal.");
            }
            element_count++;
        } while (match(TokenType::TOKEN_COMMA));
    }
    consume(TokenType::TOKEN_RIGHT_BRACKET, "Expect ']' after array elements.");
    emit_bytes(OP_BUILD_ARRAY, element_count);
    return TokenType::TOKEN_ILLEGAL;
}

TokenType Parser::map_literal(bool can_assign) {
    uint8_t element_count = 0;
    if (!check(TokenType::TOKEN_RIGHT_BRACE)) {
        do {
            consume(TokenType::TOKEN_STRING_LITERAL, "Expect string key in map literal.");
            string(false);
            consume(TokenType::TOKEN_COLON, "Expect ':' after map key.");
            expression();
            if (element_count == 255) {
                error("Cannot have more than 255 elements in a map literal.");
            }
            element_count++;
        } while (match(TokenType::TOKEN_COMMA));
    }
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expect '}' after map elements.");
    emit_bytes(OP_BUILD_MAP, element_count);
    return TokenType::TOKEN_ILLEGAL;
}

TokenType Parser::subscript(TokenType left_type, bool can_assign) {
    expression();
    consume(TokenType::TOKEN_RIGHT_BRACKET, "Expect ']' after index.");
    if (can_assign && match(TokenType::TOKEN_EQUAL)) {
        expression();
        emit_byte(OP_SET_SUBSCRIPT);
    } else {
        emit_byte(OP_GET_SUBSCRIPT);
    }
    return TokenType::TOKEN_ILLEGAL;
}

TokenType Parser::this_expression(bool can_assign) {
    if (current_compiler->enclosing == nullptr) {
        error("'this' can only be used inside a class method.");
        return TokenType::TOKEN_ILLEGAL;
    }
    emit_bytes(OP_GET_LOCAL, 0);
    return TokenType::TOKEN_CLASS;
} // espero que não tenha que adicionar mais nada ..
