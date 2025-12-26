#include "lexer.h"
#include "tokens.h"
#include <iostream>
#include <map>
#include <cctype>

using enum TokenType;

static std::map<std::string, TokenType> keywords = {
    {"print",    TokenType::TOKEN_PRINT},
    {"if",       TokenType::TOKEN_IF},
    {"else",     TokenType::TOKEN_ELSE},
    {"true",     TokenType::TOKEN_TRUE},
    {"false",    TokenType::TOKEN_FALSE},
    {"nil",      TokenType::TOKEN_NIL},
    {"and",      TokenType::TOKEN_AND},
    {"or",       TokenType::TOKEN_OR},
    {"while",    TokenType::TOKEN_WHILE},
    {"function", TokenType::TOKEN_FUNCTION},
    {"return",   TokenType::TOKEN_RETURN},
    {"import",   TokenType::TOKEN_IMPORT},
    {"this",     TokenType::TOKEN_THIS},
    {"int",      TokenType::TOKEN_INT},
    {"bool",     TokenType::TOKEN_BOOL},
    {"string",   TokenType::TOKEN_STRING},
    {"double",   TokenType::TOKEN_DOUBLE},
    {"float",    TokenType::TOKEN_FLOAT},
    {"void",     TokenType::TOKEN_VOID},
    {"class",    TokenType::TOKEN_CLASS},
};

Lexer::Lexer(const std::string& source) : source(source) {}

Token Lexer::make_token(TokenType type) {
    return {type, source.substr(start, current - start), line};
}
Token Lexer::make_token(TokenType type, const std::string& literal) {
    return {type, literal, line};
}
Token Lexer::error_token(const std::string& message) {
    return {TokenType::TOKEN_ILLEGAL, message, line};
}

bool Lexer::is_at_end() { return current >= source.length(); }
char Lexer::advance() { current++; return source[current - 1]; }
char Lexer::peek() { if (is_at_end()) return '\0'; return source[current]; }
char Lexer::peek_next() { if (current + 1 >= source.length()) return '\0'; return source[current + 1]; }
bool Lexer::match(char expected) {
    if (is_at_end() || source[current] != expected) return false;
    current++;
    return true;
}

Token Lexer::string_token() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line++;
        advance();
    }
    if (is_at_end()) return error_token("Unterminated string.");

    advance();
    return make_token(TokenType::TOKEN_STRING_LITERAL, source.substr(start + 1, current - start - 2));
}

Token Lexer::number_token() {
    while (isdigit(static_cast<unsigned char>(peek()))) advance();
    if (peek() == '.' && isdigit(static_cast<unsigned char>(peek_next()))) {
        advance();
        while (isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    return make_token(TokenType::TOKEN_NUMBER);
}

Token Lexer::identifier_token() {
    while (isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
    std::string text = source.substr(start, current - start);
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return make_token(it->second);
    }
    return make_token(TokenType::TOKEN_IDENTIFIER);
}

Token Lexer::scan_token() {
    while (true) {
        if (is_at_end()) break;
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(); // Avance sobre o espaço em branco
                break;
            case '\n':
                line++; // Incrementa a linha
                advance(); // Avance sobre a nova linha
                break;
            case '/':
                if (peek_next() == '/') {
                    // Comentário de linha: pule até o final da linha ou fim do arquivo
                    while (peek() != '\n' && !is_at_end()) {
                        advance();
                    }
                } else {
                    // Não é um comentário, é um operador de divisão.
                    // Saia do loop de pular espaços e processe-o como um token normal.
                    goto end_whitespace_and_comments_loop;
                }
                break;
            default:
                // Se não é um caractere de espaço em branco ou início de comentário,
                // significa que chegamos ao início de um token real.
                goto end_whitespace_and_comments_loop;
        }
    }
    end_whitespace_and_comments_loop:; // O label para sair do loop

    // Agora, 'start' aponta para o início do token que queremos capturar
    start = current; 

    if (is_at_end()) {
        return make_token(TokenType::TOKEN_END_OF_FILE); // Fim do arquivo
    }

    char c = advance(); 

    Token generated_token; // Para armazenar o token gerado

    // --- Lógica para tokenizar diferentes tipos ---
    if (isdigit(static_cast<unsigned char>(c))) {
        generated_token = number_token();
    } else if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
        generated_token = identifier_token();
    } else {
        switch (c) {
            // Tokens de um caractere
            case '(': generated_token = make_token(TokenType::TOKEN_LEFT_PAREN); break;
            case ')': generated_token = make_token(TokenType::TOKEN_RIGHT_PAREN); break;
            case '{': generated_token = make_token(TokenType::TOKEN_LEFT_BRACE); break;
            case '}': generated_token = make_token(TokenType::TOKEN_RIGHT_BRACE); break;
            case '[': generated_token = make_token(TokenType::TOKEN_LEFT_BRACKET); break;
            case ']': generated_token = make_token(TokenType::TOKEN_RIGHT_BRACKET); break;
            case ';': generated_token = make_token(TokenType::TOKEN_SEMICOLON); break;
            case ',': generated_token = make_token(TokenType::TOKEN_COMMA); break;
            case '.': generated_token = make_token(TokenType::TOKEN_DOT); break;
            case '+': generated_token = make_token(TokenType::TOKEN_PLUS); break;
            case '-': generated_token = make_token(TokenType::TOKEN_MINUS); break;
            case '*': generated_token = make_token(TokenType::TOKEN_STAR); break;
            // Tokens de um ou dois caracteres
            case '!': generated_token = make_token(match('=') ? TokenType::TOKEN_BANG_EQUAL : TokenType::TOKEN_BANG); break;
            case '=': generated_token = make_token(match('=') ? TokenType::TOKEN_EQUAL_EQUAL : TokenType::TOKEN_EQUAL); break;
            case '<': generated_token = make_token(match('=') ? TokenType::TOKEN_LESS_EQUAL : TokenType::TOKEN_LESS); break;
            case '>': generated_token = make_token(match('=') ? TokenType::TOKEN_GREATER_EQUAL : TokenType::TOKEN_GREATER); break;
            case '/': generated_token = make_token(TokenType::TOKEN_SLASH); break; // Agora o '/' é tratado aqui.
            // Tokens especiais como strings
            case '"': generated_token = string_token(); break;
            case '&': generated_token = make_token(match('&') ? TokenType::TOKEN_AND : TokenType::TOKEN_ILLEGAL); break;
            default:
                generated_token = error_token("Caractere inesperado.");
                break;
        }
    }
    return generated_token;
}
