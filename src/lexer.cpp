#include "lexer.h"
#include <map>

std::map<std::string, TokenType> keywords = {
    {"var", TokenType::VAR},       {"print", TokenType::PRINT},
    {"if", TokenType::IF},         {"else", TokenType::ELSE},
    {"true", TokenType::TRUE},     {"false", TokenType::FALSE},
    {"nil", TokenType::NIL},       {"and", TokenType::AND},
    {"or", TokenType::OR}
};

Lexer::Lexer(std::string source) : source(std::move(source)) {}

std::vector<Token> Lexer::scan_tokens() {
    while (!is_at_end()) {
        start = current;
        scan_token();
    }
    tokens.push_back({TokenType::END_OF_FILE, ""});
    return tokens;
}

void Lexer::scan_token() {
    char c = advance();
    switch (c) {
        case '(': add_token(TokenType::LEFT_PAREN); break;
        case ')': add_token(TokenType::RIGHT_PAREN); break;
        case '{': add_token(TokenType::LEFT_BRACE); break;
        case '}': add_token(TokenType::RIGHT_BRACE); break;
        case ';': add_token(TokenType::SEMICOLON); break;
        case '+': add_token(TokenType::PLUS); break;
        case '-': add_token(TokenType::MINUS); break;
        case '*': add_token(TokenType::STAR); break;
        case '!': add_token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG); break;
        case '=': add_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL); break;
        case '<': add_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS); break;
        case '>': add_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER); break;
        
        // CORREÇÃO PRINCIPAL AQUI
        case '/':
            if (match('/')) {
                // É um comentário. Avance até o final da linha, mas não adicione tokens.
                while (peek() != '\n' && !is_at_end()) {
                    advance();
                }
            } else {
                // Se não, é só um operador de divisão normal
                add_token(TokenType::SLASH);
            }
            break;
        
        case ' ': case '\r': case '\t': case '\n': break;
        
        case '"':
            while (peek() != '"' && !is_at_end()) advance();
            if (is_at_end()) { /* Ignora string não terminada por enquanto */ }
            else {
                advance(); // O " de fechamento
                add_token(TokenType::STRING, source.substr(start + 1, current - start - 2));
            }
            break;

        default:
            if (isdigit(c)) {
                while (isdigit(peek())) advance();
                if (peek() == '.' && isdigit(peek_next())) {
                    advance();
                    while (isdigit(peek())) advance();
                }
                add_token(TokenType::NUMBER);
            } else if (isalpha(c) || c == '_') {
                while (isalnum(peek()) || peek() == '_') advance();
                std::string text = source.substr(start, current - start);
                auto it = keywords.find(text);
                if (it != keywords.end()) {
                    add_token(it->second);
                } else {
                    add_token(TokenType::IDENTIFIER);
                }
            } else {
                add_token(TokenType::ILLEGAL);
            }
            break;
    }
}

// --- Funções de Ajuda (sem alterações) ---
bool Lexer::is_at_end() { return current >= source.length(); }
char Lexer::advance() { return source[current++]; }
void Lexer::add_token(TokenType type) { add_token(type, source.substr(start, current - start)); }
void Lexer::add_token(TokenType type, const std::string& literal) { tokens.push_back({type, literal}); }
bool Lexer::match(char expected) { if (is_at_end() || source[current] != expected) return false; current++; return true; }
char Lexer::peek() { if (is_at_end()) return '\0'; return source[current]; }
char Lexer::peek_next() { if (current + 1 >= source.length()) return '\0'; return source[current + 1]; }