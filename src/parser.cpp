#include "parser.h"
#include "opcodes.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include "object.h"
#include "debug.h"

static bool types_are_compatible(TokenType variable_type, TokenType value_type) {
    // Um tipo é sempre compatível com ele mesmo.
    if (variable_type == value_type) {
        return true;
    }

    // --- Novas Regras de Compatibilidade Numérica ---
    // Vamos considerar que qualquer tipo numérico pode ser atribuído a qualquer
    // variável de tipo numérico por enquanto.
    bool var_is_numeric = (variable_type == TokenType::INT || 
                           variable_type == TokenType::DOUBLE || 
                           variable_type == TokenType::FLOAT);

    bool val_is_numeric = (value_type == TokenType::INT || 
                           value_type == TokenType::DOUBLE || 
                           value_type == TokenType::FLOAT);

    if (var_is_numeric && val_is_numeric) {
        return true;
    }
    // --- Fim das Novas Regras ---

    return false;
}

// Estrutura do Parser
Parser::Parser(Lexer& lexer, Compiler* compiler) : lexer(lexer), current_compiler(compiler) {
    had_error = false;
    panic_mode = false;
    
    // Inicializa o lookahead. Preenche current e next.
    advance();
    advance();
    
    initialize_rules();
}

// Funções de erro
void Parser::error_at(const Token& token, const std::string& message) {
    if (panic_mode) return;
    panic_mode = true;
    std::cerr << "[linha " << token.line << "] Error";
    if (token.type == TokenType::END_OF_FILE) {
        std::cerr << " in the end";
    } else {
        std::cerr << " in '" << token.literal << "'";
    }
    std::cerr << ": " << message << std::endl;
    had_error = true;
}
void Parser::error(const std::string& message) { error_at(previous, message); }
void Parser::error_at_current(const std::string& message) { error_at(current, message); }


// Funções de controle de tokens
void Parser::advance() {
    previous = current;
    current = next;

    for (;;) {
        next = lexer.scan_token();
        if (next.type != TokenType::ILLEGAL) break;
        error_at_current("Invalid character: " + next.literal);
    }
}
bool Parser::check_next(TokenType type) {
    return next.type == type;
}
void Parser::consume(TokenType type, const std::string& message) {
    if (current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}
bool Parser::check(TokenType type) { return current.type == type; }
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
uint8_t Parser::make_constant(const SapphireValue& value) {
    int constant = current_chunk()->add_constant(value);
    if (constant > 255) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}
void Parser::emit_constant(const SapphireValue& value) {
    emit_bytes(OP_CONSTANT, make_constant(value));
}

int Parser::emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff); // Placeholder de 16 bits
    emit_byte(0xff);
    return current_chunk()->code.size() - 2;
}

void Parser::patch_jump(int offset) {
    int jump = current_chunk()->code.size() - offset - 1;

    if (jump > UINT16_MAX) {
        error("Jump is too long to be encoded.");
    }

    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

uint8_t Parser::argument_list() {
    uint8_t arg_count = 0;
    if (!check(TokenType::RIGHT_PAREN)) { // Não precisa mais de "parser->"
        do {
            expression(); // Não precisa mais de "parser->"
            if (arg_count == 255) {
                error("You can't have more than 225 arguments!"); // Não precisa mais de "parser->"
            }
            arg_count++;
        } while (match(TokenType::COMMA)); // Não precisa mais de "parser->"
    }
    consume(TokenType::RIGHT_PAREN, "Expected ')' after argument.");
    return arg_count;
}

// Lógica principal do Parser
TokenType Parser::parse_precedence(Precedence precedence) {
    advance();
    PrefixParseFn prefix_rule = get_rule(previous.type)->prefix;

    if (!prefix_rule) {
        error("Expected expression.");
        return TokenType::ILLEGAL;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    TokenType left_type = prefix_rule(can_assign);

    while (precedence <= get_rule(current.type)->precedence) {
        advance();
        InfixParseFn infix_rule = get_rule(previous.type)->infix;
        // PASSA O TIPO ESQUERDO para a regra infixa!
        left_type = infix_rule(left_type, can_assign);
    }
    
    if (can_assign && match(TokenType::EQUAL)) {
        error("Invalid assignment target.");
    }

    return left_type;
}
ParseRule* Parser::get_rule(TokenType type) {
    if (rules.count(type)) return &rules[type];
    return &rules[TokenType::ILLEGAL]; // Retorna uma regra segura
}

// Lógica de declaração de variáveis
uint8_t Parser::identifier_constant(const Token& name) {
    return make_constant(new_string(name.literal));
}

void Parser::add_local(Token name, TokenType type) {
    if (current_compiler->local_count == 256) {
        error("Too many local variables in a function!");
        return;
    }
    Local* local = &current_compiler->locals[current_compiler->local_count++];
    local->name = name;
    local->depth = -1; // -1 = não inicializada
    local->type = type; // <<< PRONTO! O tipo foi armazenado!
}

int Parser::resolve_local(Compiler* compiler, const Token& name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (name.literal == local->name.literal) {
            if (local->depth == -1) {
                // Agora isso funciona porque 'error' não está sendo chamada de um contexto estático
                error("Cannot read a local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}


void Parser::declare_variable(const Token& name, TokenType type) {
    if (current_compiler->scope_depth == 0) {
        return;
    }

    for (int i = current_compiler->local_count - 1; i >= 0; i--) {
        Local* local = &current_compiler->locals[i];
        if (local->depth != -1 && local->depth < current_compiler->scope_depth) {
            break;
        }
        if (name.literal == local->name.literal) {
            error("Variable with that name already declared in this scope.");
        }
    }

    add_local(name, type);
}

uint8_t Parser::parse_variable(const std::string& error_message, TokenType type) {
    consume(TokenType::IDENTIFIER, error_message);

    // Passa o tipo para a função que declara a variável no escopo atual
    declare_variable(previous, type);

    if (current_compiler->scope_depth > 0) return 0;
    return identifier_constant(previous);
}
void Parser::mark_initialized() {
    if (current_compiler->scope_depth == 0) return;
    current_compiler->locals[current_compiler->local_count - 1].depth = current_compiler->scope_depth;
}

void Parser::define_variable(uint8_t global) {
    if (current_compiler->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit_bytes(OP_DEFINE_GLOBAL, global);
}


// Parsing de statements e declarações
TokenType Parser::expression() {
    return parse_precedence(PREC_ASSIGNMENT);
}

void Parser::block() {
    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END_OF_FILE)) {
        declaration();
    }
    consume(TokenType::RIGHT_BRACE, "Esperava '}' depois do bloco.");
}

void Parser::begin_scope() { current_compiler->scope_depth++; }
void Parser::end_scope() {
    current_compiler->scope_depth--;
    while (current_compiler->local_count > 0 && current_compiler->locals[current_compiler->local_count - 1].depth > current_compiler->scope_depth) {
        emit_byte(OP_POP);
        current_compiler->local_count--;
    }
}

void Parser::print_statement() {
    expression();
    consume(TokenType::SEMICOLON, "Esperava ';' depois do valor do print.");
    emit_byte(OP_PRINT);
}

void Parser::if_statement() {
    consume(TokenType::LEFT_PAREN, "Esperava '(' depois de 'if'.");
    expression();
    consume(TokenType::RIGHT_PAREN, "Esperava ')' depois da condicao."); 

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);

    statement();

    int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (match(TokenType::ELSE)) {
        statement();
    }
    patch_jump(else_jump);
}


void Parser::emit_loop(int loop_start) {
    emit_byte(OP_LOOP);
    
    // A correção está aqui: sem o '+ 2'
    int offset = current_chunk()->code.size() - loop_start; 
    
    if (offset > UINT16_MAX) {
        error("Loop too long!");
    }
    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

void Parser::while_statement() {
    int loop_start = current_chunk()->code.size();
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'while'.");
    expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after condition.");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    
    emit_byte(OP_POP); // Remove a condição para executar o corpo
    statement();
    emit_loop(loop_start); // Salta de volta para o início do loop

    patch_jump(exit_jump);
    emit_byte(OP_POP); // Remove a condição quando o loop termina
}

void Parser::expression_statement() {
    expression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression.");
    emit_byte(OP_POP);
}
void Parser::declaration() {
    if (match(TokenType::CLASS)) {
        class_declaration();
    } else if (match(TokenType::FUNCTION)) {
        function_declaration();
    } 
    // Lógica robusta com lookahead:
    // Uma declaração de variável é um TIPO seguido por um NOME.
    // TIPO pode ser uma palavra-chave ou um nome de classe (IDENTIFIER).
    // NOME é sempre um IDENTIFIER.
    else if (check(TokenType::INT) || check(TokenType::BOOL) || check(TokenType::STRING) ||
             check(TokenType::DOUBLE) || check(TokenType::FLOAT) || check(TokenType::VOID) ||
             (check(TokenType::IDENTIFIER) && check_next(TokenType::IDENTIFIER)))
    {
        declaration_statement();
    }
    else {
        statement();
    }

    if (panic_mode) {
        synchronize();
    }
}
void Parser::class_declaration() {
    // 1. SETUP INICIAL DA CLASSE
    consume(TokenType::IDENTIFIER, "Expect class name.");
    Token class_name = previous;
    uint8_t name_constant = identifier_constant(class_name);

    // Registra o nome da classe como um tipo conhecido no escopo global.
    // Isso é crucial para que 'Ponto p = Ponto();' funcione.
    current_compiler->global_types[class_name.literal] = TokenType::CLASS;
    declare_variable(class_name, TokenType::CLASS);

    // Cria o objeto da classe em tempo de compilação, que será preenchido.
    ObjClass* klass = new_class(new_string(class_name.literal));

    consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");

    // 2. LOOP PARA PARSEAR O CORPO DA CLASSE (CAMPOS E MÉTODOS)
    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END_OF_FILE)) {
        // Se o token for 'function', é um método.
        if (check(TokenType::FUNCTION)) {
            match(TokenType::FUNCTION);
            
            // --- Lógica de Compilação de Método Robusta ---
            if (!(match(TokenType::INT) || match(TokenType::BOOL) || match(TokenType::STRING) ||
                  match(TokenType::DOUBLE) || match(TokenType::FLOAT) || match(TokenType::VOID))) {
                error("Expect method return type (int, bool, string, void, etc.).");
            }
            TokenType return_type = previous.type;

            consume(TokenType::IDENTIFIER, "Expect method name.");
            Token method_name = previous;

            // Cria um compilador aninhado para o escopo do método
            Compiler method_compiler(new_function());
            method_compiler.enclosing = current_compiler;
            current_compiler = &method_compiler;
            current_compiler->function->name = new_string(method_name.literal);
            current_compiler->function_return_type = return_type;
            
            begin_scope();

            // Analisa parâmetros
            consume(TokenType::LEFT_PAREN, "Expect '(' after method name.");
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    current_compiler->function->arity++;
                    if (current_compiler->function->arity > 255) {
                        error_at_current("Can't have more than 255 parameters.");
                    }
                    if (!(match(TokenType::INT) || match(TokenType::BOOL) || match(TokenType::STRING) || match(TokenType::DOUBLE) || match(TokenType::FLOAT))) {
                        error_at_current("Expect parameter type.");
                    }
                    uint8_t param_const = parse_variable("Expect parameter name.", previous.type);
                    define_variable(param_const);
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");

            // Analisa o corpo do método
            consume(TokenType::LEFT_BRACE, "Expect '{' before method body.");
            block();

            // Finaliza a compilação do método e captura a função compilada
            ObjFunction* function = end_compiler_scope();
            
            // Adiciona o método à classe
            klass->methods[method_name.literal] = new_closure(function);

        } else {
            // Se não for 'function', é uma declaração de campo.
            field_declaration();
        }
    }

    consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");

    // 3. FINALIZAÇÃO
    // Emite a classe (agora completa com todos os métodos) como uma constante.
    emit_bytes(OP_CONSTANT, make_constant(klass));
    
    // Emite o bytecode para definir a variável da classe em tempo de execução.
    define_variable(name_constant);
}
void Parser::statement() {
    if (match(TokenType::PRINT)) {
        print_statement();
    } else if (match(TokenType::RETURN)) {
        return_statement();
    } else if (match(TokenType::LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (match(TokenType::IF)) {
        if_statement();
    } else if (match(TokenType::WHILE)) {
        while_statement();
    } else {
        expression_statement();
    }
}

void Parser::synchronize() {
    panic_mode = false;
    while (current.type != TokenType::END_OF_FILE) {
        if (previous.type == TokenType::SEMICOLON) return;
        switch(current.type) {
            case TokenType::INT: 
            case TokenType::BOOL:
            case TokenType::STRING:
            case TokenType::DOUBLE:
            case TokenType::FLOAT:
            case TokenType::FUNCTION:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::PRINT:
            case TokenType::RETURN:
                return;
            default:
                ; // Não faz nada
        }
        advance();
    }
}

void Parser::declaration_statement() {

    // 1. Pega o tipo do token atual e ENTÃO avança para o próximo.
    TokenType var_type = current.type;
    advance(); // Consome o token do tipo (ex: 'double' ou 'Ponto').

    // 2. Verifica se é uma declaração de array (nova lógica).
    if (match(TokenType::LEFT_BRACKET)) {
        consume(TokenType::RIGHT_BRACKET, "Expect ']' after '[' in array type specifier.");
    }
    
    // 3. Trata nomes de classe como um tipo CLASS.
    if (var_type == TokenType::IDENTIFIER) {
        var_type = TokenType::CLASS;
    }

    // 4. Consome o nome da variável.
    consume(TokenType::IDENTIFIER, "Expected variable name after type specifier.");
    Token var_name = previous;

    // 5. O resto da função continua como antes.
    declare_variable(var_name, var_type);

    if (match(TokenType::EQUAL)) {
        TokenType rhs_type = expression();
        if (!types_are_compatible(var_type, rhs_type)) {
            // A verificação agora funciona para literais de array também.
            if (!(var_type == TokenType::CLASS && rhs_type == TokenType::CLASS) && !(rhs_type == TokenType::ILLEGAL)) {
                 error("Incompatible types: Cannot assign this expression to the variable.");
            }
        }
    } else {
        emit_byte(OP_NIL);
    }

    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration.");

    define_variable(identifier_constant(var_name));
}
// Funções de parsing para cada tipo de token
TokenType Parser::grouping(bool can_assign) {
    // O tipo de um agrupamento é o tipo da expressão dentro dele
    TokenType type = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
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

    return TokenType::DOUBLE;
}

void Parser::function_declaration() {
    // Primeiro, precisamos saber o tipo de retorno da função.
    // Ele vem logo após a palavra-chave 'function'.
    if (!(match(TokenType::INT) || match(TokenType::BOOL) || match(TokenType::STRING) ||
          match(TokenType::DOUBLE) || match(TokenType::FLOAT) || match(TokenType::VOID))) {
        error("Expect function return type (int, bool, string, void, etc.).");
    }
    TokenType return_type = previous.type;

    uint8_t global = parse_variable("Expect function name.", TokenType::FUNCTION);
    mark_initialized();
    
    // CORREÇÃO: Passa o tipo de retorno que acabamos de ler para a função 'function'.
    function(TokenType::FUNCTION, return_type);
    
    define_variable(global);
}


ObjFunction* Parser::end_compiler_scope() {
    // Garante que toda função tem um retorno implícito no final
    emit_return(); 
    
    ObjFunction* function = current_compiler->function;

    // Se DEBUG_PRINT_CODE estiver ativo, imprime o bytecode da função compilada
    #ifdef DEBUG_PRINT_CODE
        if (!had_error) { // Acessa o 'had_error' da classe Parser
            disassemble_chunk(function->chunk, function->name != nullptr ? function->name->chars : "<script>");
        }
    #endif

    // Restaura o compilador anterior e retorna a função compilada
    current_compiler = current_compiler->enclosing;
    return function;
}

// A função principal de compilação de função
void Parser::function(TokenType kind, TokenType return_type) {
    // 1. Cria um novo compilador para esta função, aninhado ao anterior
    Compiler compiler(new_function());
    compiler.enclosing = current_compiler;
    compiler.function_return_type = return_type;
    current_compiler = &compiler;
    
    // 2. O nome da função já foi consumido, agora o atribuímos ao objeto de função
    current_compiler->function->name = new_string(previous.literal);

    // 3. Abre um novo escopo para os parâmetros
    begin_scope();

    // 4. Analisa a lista de parâmetros
    consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            current_compiler->function->arity++;
            if (current_compiler->function->arity > 255) {
                error_at_current("Can't have more than 255 parameters.");
            }
            
            // Consome o tipo do parâmetro
            if (!(match(TokenType::INT) || match(TokenType::BOOL) || match(TokenType::STRING) ||
                  match(TokenType::DOUBLE) || match(TokenType::FLOAT))) {
                error_at_current("Expect parameter type.");
            }
            TokenType param_type = previous.type;

            // Consome e declara o nome do parâmetro como uma variável local
            uint8_t param_const = parse_variable("Expect parameter name.", param_type);
            define_variable(param_const);

        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");

    // 5. Analisa o corpo da função
    consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
    block();

    // 6. Finaliza a compilação desta função
    ObjFunction* func = end_compiler_scope(); 

    // 7. Emite o bytecode para criar a função em tempo de execução
    emit_bytes(OP_CLOSURE, make_constant(func));
}
void Parser::return_statement() {
    if (current_compiler->function_return_type == TokenType::VOID) {
        if (!match(TokenType::SEMICOLON)) {
            error("A 'void' function cannot return a value.");
        }
        emit_return();
    } else {
        if (match(TokenType::SEMICOLON)) {
            error("A non-void function must return a value.");
            emit_return();
        } else {
            TokenType value_type = expression();
            if (!types_are_compatible(current_compiler->function_return_type, value_type)) {
                error("Return value type does not match function return type.");
            }
            consume(TokenType::SEMICOLON, "Expect ';' after return value.");
            emit_byte(OP_RETURN);
        }
    }
}

// Modifique a função 'string' para retornar um tipo
TokenType Parser::string(bool can_assign) {
    emit_constant(new_string(previous.literal));
    return TokenType::STRING;
}
TokenType Parser::call(TokenType left_type, bool can_assign) {
    uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);

    // Se a chamada for em uma classe, é um construtor. Retorna uma instância da classe.
    if (left_type == TokenType::CLASS) {
        return TokenType::CLASS;
    }

    return TokenType::ILLEGAL;
}
TokenType Parser::unary(bool can_assign) {
    TokenType operator_type = previous.type;
    
    // Analisa o operando
    TokenType operand_type = parse_precedence(PREC_UNARY);

    // Emite o bytecode para a operação
    switch (operator_type) {
        case TokenType::MINUS: emit_byte(OP_NEGATE); break;
        case TokenType::BANG:  emit_byte(OP_NOT); break;
        default: break;
    }
    
    // O tipo de uma negação numérica é número, de uma negação lógica é booleano.
    if (operator_type == TokenType::MINUS) {
        return TokenType::DOUBLE; // Ou o tipo do operando, se tivéssemos checagem aqui
    }
    return TokenType::BOOL;
}
static TokenType resolve_local_type(Compiler* compiler, const Token& name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (name.literal == local->name.literal) {
            // Retorna o tipo armazenado na tabela de símbolos!
            return local->type;
        }
    }
    return TokenType::ILLEGAL;
}
TokenType Parser::binary(TokenType left_type, bool can_assign) {
    TokenType operator_type = previous.type;
    ParseRule* rule = get_rule(operator_type);
    TokenType right_type = parse_precedence((Precedence)(rule->precedence + 1));

    // A lógica de verificação de erro está boa, não precisa mudar.
    if (left_type != TokenType::ILLEGAL && right_type != TokenType::ILLEGAL) {
        bool left_is_numeric = (left_type == TokenType::INT || left_type == TokenType::DOUBLE || left_type == TokenType::FLOAT);
        bool right_is_numeric = (right_type == TokenType::INT || right_type == TokenType::DOUBLE || right_type == TokenType::FLOAT);
        switch (operator_type) {
            case TokenType::PLUS:
                if (!((left_is_numeric && right_is_numeric) || (left_type == TokenType::STRING && right_type == TokenType::STRING))) {
                    error("The '+' operator requires two numbers or two strings.");
                }
                break;
            case TokenType::MINUS: case TokenType::STAR: case TokenType::SLASH:
                if (!left_is_numeric || !right_is_numeric) {
                    error("Operands for '-', '*', '/' must be numbers.");
                }
                break;
            case TokenType::GREATER: case TokenType::GREATER_EQUAL: case TokenType::LESS: case TokenType::LESS_EQUAL: case TokenType::EQUAL_EQUAL: case TokenType::BANG_EQUAL:
                 if (!left_is_numeric || !right_is_numeric) {
                    error("Operands for comparisons must be numbers for now.");
                 }
                break;
            default: break;
        }
    }

    // A emissão de bytecode também está boa.
    switch (operator_type) {
        case TokenType::PLUS:          emit_byte(OP_ADD); break;
        case TokenType::MINUS:         emit_byte(OP_SUBTRACT); break;
        case TokenType::STAR:          emit_byte(OP_MULTIPLY); break;
        case TokenType::SLASH:         emit_byte(OP_DIVIDE); break;
        case TokenType::BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT); break;
        case TokenType::EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
        case TokenType::GREATER:       emit_byte(OP_GREATER); break;
        case TokenType::GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
        case TokenType::LESS:          emit_byte(OP_LESS); break;
        case TokenType::LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
        default: return TokenType::ILLEGAL;
    }

    switch (operator_type) {
        case TokenType::PLUS:
            if (left_type == TokenType::STRING && right_type == TokenType::STRING) return TokenType::STRING;
            // Intencionalmente continua para a lógica numérica se não for string...

        case TokenType::MINUS:
        case TokenType::STAR:
        case TokenType::SLASH:
            { // Usamos chaves para criar um escopo para as variáveis
                bool left_is_numeric = (left_type == TokenType::INT || left_type == TokenType::DOUBLE || left_type == TokenType::FLOAT);
                bool right_is_numeric = (right_type == TokenType::INT || right_type == TokenType::DOUBLE || right_type == TokenType::FLOAT);

                // Só retorna um tipo numérico se AMBOS forem numéricos.
                if (left_is_numeric && right_is_numeric) {
                    if (left_type == TokenType::DOUBLE || right_type == TokenType::DOUBLE) return TokenType::DOUBLE;
                    return TokenType::INT;
                }
                // Se a combinação for inválida (ex: STRING + INT), retorna ILLEGAL.
                return TokenType::ILLEGAL;
            }

        case TokenType::BANG_EQUAL:
        case TokenType::EQUAL_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
            return TokenType::BOOL;

        default:
            return TokenType::ILLEGAL;
    }
}
TokenType Parser::literal(bool can_assign) {
    switch (previous.type) {
        case TokenType::FALSE: emit_byte(OP_FALSE); return TokenType::BOOL;
        case TokenType::TRUE:  emit_byte(OP_TRUE);  return TokenType::BOOL;
        case TokenType::NIL:   emit_byte(OP_NIL);   return TokenType::NIL;
        default: return TokenType::ILLEGAL; // Não deve acontecer
    }
}
TokenType Parser::variable(bool can_assign) {
    Token name = previous;
    
    uint8_t get_op, set_op;
    int arg;
    TokenType var_type = TokenType::ILLEGAL;

    arg = resolve_local(current_compiler, name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
        var_type = resolve_local_type(current_compiler, name);
    } else {
        arg = identifier_constant(name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
        auto it = current_compiler->global_types.find(name.literal);
        if (it != current_compiler->global_types.end()) {
            var_type = it->second;
        }
    }

    if (can_assign && match(TokenType::EQUAL)) {
        TokenType assigned_type = expression();
        if (var_type != TokenType::ILLEGAL && !types_are_compatible(var_type, assigned_type)) {
            error("Incompatible types for assignment.");
        }
        emit_bytes(set_op, (uint8_t)arg);
        return assigned_type;
    }

    emit_bytes(get_op, (uint8_t)arg);
    return var_type;
}

// Inicialização da tabela de regras
void Parser::initialize_rules() {
    rules[TokenType::LEFT_PAREN]    = { [this](bool b){ return grouping(b); },      [this](TokenType lhs_type, bool b){ return call(lhs_type, b); }, PREC_CALL };
    rules[TokenType::DOT]           = { nullptr,                                    [this](TokenType lhs_type, bool b){ return dot(lhs_type, b); },    PREC_CALL };
    rules[TokenType::MINUS]         = { [this](bool b){ return unary(b); },         [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_TERM };
    rules[TokenType::PLUS]          = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_TERM };
    rules[TokenType::SLASH]         = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_FACTOR };
    rules[TokenType::STAR]          = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_FACTOR };
    rules[TokenType::EQUAL_EQUAL]   = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_EQUALITY };
    rules[TokenType::BANG_EQUAL]    = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_EQUALITY };
    rules[TokenType::GREATER]       = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_COMPARISON };
    rules[TokenType::GREATER_EQUAL] = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_COMPARISON };
    rules[TokenType::LESS]          = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_COMPARISON };
    rules[TokenType::LESS_EQUAL]    = { nullptr,                                    [this](TokenType lhs_type, bool b){ return binary(lhs_type, b); }, PREC_COMPARISON };
    rules[TokenType::BANG]          = { [this](bool b){ return unary(b); },         nullptr, PREC_NONE };
    rules[TokenType::IDENTIFIER]    = { [this](bool b){ return variable(b); },      nullptr, PREC_NONE };
    rules[TokenType::NUMBER]        = { [this](bool b){ return number(b); },         nullptr, PREC_NONE };
    rules[TokenType::STRING_LITERAL]= { [this](bool b){ return string(b); },         nullptr, PREC_NONE };
    rules[TokenType::FALSE]         = { [this](bool b){ return literal(b); },       nullptr, PREC_NONE };
    rules[TokenType::TRUE]          = { [this](bool b){ return literal(b); },        nullptr, PREC_NONE };
    rules[TokenType::NIL]           = { [this](bool b){ return literal(b); },        nullptr, PREC_NONE };
    rules[TokenType::RIGHT_PAREN]   = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::LEFT_BRACKET]  = { [this](bool b){ return array_literal(b); }, [this](TokenType l, bool b){ return subscript(l, b); }, PREC_CALL };
    rules[TokenType::LEFT_BRACE]    = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::RIGHT_BRACE]   = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::COMMA]         = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::EQUAL]         = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::SEMICOLON]     = { nullptr, nullptr, PREC_NONE };
    rules[TokenType::THIS]          = { [this](bool b){ return this_expression(b); },nullptr, PREC_NONE };
}

// Adicione a implementação da nova função 'dot'
TokenType Parser::dot(TokenType left_type, bool can_assign) {
    consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifier_constant(previous);

    if (can_assign && match(TokenType::EQUAL)) {
        // Atribuição: p.x = 10
        TokenType rhs_type = expression(); // Analisa o lado direito
        emit_bytes(OP_SET_PROPERTY, name);
        // O tipo de uma expressão de atribuição é o tipo do valor atribuído
        return rhs_type;
    }
    
    // Leitura: print p.x
    emit_bytes(OP_GET_PROPERTY, name);
    // Em tempo de compilação, não temos como saber o tipo de uma propriedade ainda.
    return TokenType::ILLEGAL; 
}
TokenType Parser::array_literal(bool can_assign) {
    uint8_t element_count = 0;
    if (!check(TokenType::RIGHT_BRACKET)) {
        do {
            expression(); // Compila a expressão do elemento, deixando o valor na pilha
            if (element_count == 255) {
                error("Cannot have more than 255 elements in an array literal.");
            }
            element_count++;
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_BRACKET, "Expect ']' after array elements.");
    
    emit_bytes(OP_BUILD_ARRAY, element_count);

    // O tipo de um literal de array é, bem, um array.
    // Precisaremos de um tipo para isso no futuro, por enquanto ILLEGAL funciona.
    return TokenType::ILLEGAL; // TODO: Criar um tipo de array
}
TokenType Parser::subscript(TokenType left_type, bool can_assign) {
    // Analisa a expressão do índice, como antes
    expression();
    consume(TokenType::RIGHT_BRACKET, "Expect ']' after index.");

    // --- NOVA LÓGICA DE ATRIBUIÇÃO ---
    // Se o próximo token for um '=', então é uma atribuição.
    if (can_assign && match(TokenType::EQUAL)) {
        // Analisa a expressão do valor a ser atribuído (o lado direito do '=')
        expression();
        // Emite o novo opcode para definir o valor no índice
        emit_byte(OP_SET_SUBSCRIPT);
    } else {
        // Se não for uma atribuição, é apenas uma leitura, como antes.
        emit_byte(OP_GET_SUBSCRIPT);
    }
    
    // O tipo do resultado é desconhecido em tempo de compilação.
    return TokenType::ILLEGAL;
}
TokenType Parser::this_expression(bool can_assign) {
    // Verificação de segurança: 'this' só pode ser usado dentro de um método de classe.
    // Sabemos que estamos em um método se o compilador atual tem um "enclosing" (o compilador da função principal).
    if (current_compiler->enclosing == nullptr) {
        error("'this' can only be used inside a class method.");
        return TokenType::ILLEGAL;
    }

    // A mágica acontece aqui. 'this' é apenas uma variável local que a VM
    // convenientemente coloca no slot 0 para nós.
    emit_bytes(OP_GET_LOCAL, 0);

    // O tipo de uma expressão 'this' é a própria classe.
    return TokenType::CLASS;
}
void Parser::field_declaration() {
    // Consome o tipo (int, double, etc. ou um nome de classe)
    advance(); 
    // Consome o nome do campo
    consume(TokenType::IDENTIFIER, "Expect field name.");
    // Consome o ponto e vírgula
    consume(TokenType::SEMICOLON, "Expect ';' after field declaration.");
}