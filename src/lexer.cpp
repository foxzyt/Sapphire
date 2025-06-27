#include "lexer.h"
#include <iostream>
#include <map>
#include <cctype> // Para isdigit, isalpha, isalnum

// Mapa de palavras-chave
static std::map<std::string, TokenType> keywords = {
    {"print", TokenType::PRINT},
    {"if", TokenType::IF},         {"else", TokenType::ELSE},
    {"true", TokenType::TRUE},     {"false", TokenType::FALSE},
    {"nil", TokenType::NIL},       {"and", TokenType::AND},
    {"or", TokenType::OR},         {"while", TokenType::WHILE},
    {"function", TokenType::FUNCTION}, {"return", TokenType::RETURN},
    {"import", TokenType::IMPORT},  {"this", TokenType::THIS},
    {"int", TokenType::INT},         
    {"bool", TokenType::BOOL},       
    {"string", TokenType::STRING},   
    {"double", TokenType::DOUBLE},
    {"float", TokenType::FLOAT},
    {"void", TokenType::VOID},
    {"class", TokenType::CLASS},
};

Lexer::Lexer(const std::string& source) : source(source) {}

// Funções de ajuda para criar tokens
Token Lexer::make_token(TokenType type) {
    return {type, source.substr(start, current - start), line};
}
Token Lexer::make_token(TokenType type, const std::string& literal) {
    return {type, literal, line};
}
Token Lexer::error_token(const std::string& message) {
    return {TokenType::ILLEGAL, message, line};
}

// Funções de ajuda para navegar o código
bool Lexer::is_at_end() { return current >= source.length(); }
char Lexer::advance() { current++; return source[current - 1]; }
char Lexer::peek() { if (is_at_end()) return '\0'; return source[current]; }
char Lexer::peek_next() { if (current + 1 >= source.length()) return '\0'; return source[current + 1]; }
bool Lexer::match(char expected) {
    if (is_at_end() || source[current] != expected) return false;
    current++;
    return true;
}

void skip_whitespace() {
    // Implementação de pular espaços em branco, se necessário
}

// Funções de tokenização específicas
Token Lexer::string_token() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line++;
        advance();
    }
    if (is_at_end()) return error_token("Unterminated string.");
    
    advance(); // Consome o " de fechamento

    // ----- DEBUG PRINTS -----
    std::cout << "--- DEBUG LEXER (string_token) ---" << std::endl;
    std::cout << "start index: " << start << std::endl;
    std::cout << "current index: " << current << std::endl;
    std::cout << "Calculated length: " << (current - start - 2) << std::endl;
    std::cout << "Substring value: '" << source.substr(start + 1, current - start - 2) << "'" << std::endl;
    std::cout << "------------------------------------" << std::endl;
    // ----- FIM DO DEBUG -----

    return make_token(TokenType::STRING_LITERAL, source.substr(start + 1, current - start - 2));
}

Token Lexer::number_token() {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek())) advance();
    }
    return make_token(TokenType::NUMBER);
}

Token Lexer::identifier_token() {
    while (isalnum(peek()) || peek() == '_') advance();
    std::string text = source.substr(start, current - start);
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return make_token(it->second);
    }
    return make_token(TokenType::IDENTIFIER);
}


Token Lexer::scan_token() {
    // Pular espaços em branco e comentários
    while (true) {
        if (is_at_end()) break;
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else {
                    goto end_whitespace_loop;
                }
                break;
            default:
                goto end_whitespace_loop;
        }
    }
    end_whitespace_loop:;


    start = current;
    if (is_at_end()) return make_token(TokenType::END_OF_FILE);

    char c = advance();

    if (isdigit(c)) return number_token();
    if (isalpha(c) || c == '_') return identifier_token();

    switch (c) {
        case '(': return make_token(TokenType::LEFT_PAREN);
        case ')': return make_token(TokenType::RIGHT_PAREN);
        case '{': return make_token(TokenType::LEFT_BRACE);
        case '}': return make_token(TokenType::RIGHT_BRACE);
        case '[': return make_token(TokenType::LEFT_BRACKET);
        case ']': return make_token(TokenType::RIGHT_BRACKET);
        case ';': return make_token(TokenType::SEMICOLON);
        case ',': return make_token(TokenType::COMMA);
        case '.': return make_token(TokenType::DOT);
        case '+': return make_token(TokenType::PLUS);
        case '-': return make_token(TokenType::MINUS);
        case '*': return make_token(TokenType::STAR);
        case '/': return make_token(TokenType::SLASH);
        case '!': return make_token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
        case '=': return make_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
        case '<': return make_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
        case '>': return make_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
        case '"': return string_token();
    }

    return error_token("Caractere inesperado.");
}