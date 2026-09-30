#include "Lexer.h"
#include <cctype>

Lexer::Lexer(std::string src) : source(std::move(src)) {
    keywords = {
        {"var", KW_VAR}, {"if", KW_IF}, {"else", KW_ELSE},
        {"while", KW_WHILE}, {"for", KW_FOR}, {"return", KW_RETURN}
    };
}

bool Lexer::isAtEnd() const { return cursor >= source.length(); }

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[cursor];
}

char Lexer::peekNext() const {
    if (cursor + 1 >= source.length()) return '\0';
    return source[cursor + 1];
}

char Lexer::advance() {
    char c = source[cursor++];
    column++;
    return c;
}

std::vector<Token> Lexer::scanTokens() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        char c = peek();

        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            line++; column = 1; advance();
        } else if (c == '/') {
            if (peekNext() == '/') {
                while (peek() != '\n' && !isAtEnd()) advance();
            } else if (peekNext() == '*') {
                advance(); advance();
                while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                    if (peek() == '\n') { line++; column = 1; }
                    advance();
                }
                if (!isAtEnd()) { advance(); advance(); }
            } else {
                int startCol = column; advance();
                tokens.push_back({OPERATOR_SLASH, "/", line, startCol});
            }
        } else if (isalpha(c) || c == '_') {
            int startCol = column;
            std::string acc = "";
            while (isalnum(peek()) || peek() == '_') acc += advance();
            TokenType type = keywords.count(acc) ? keywords[acc] : TOKEN_IDENTIFIER;
            tokens.push_back({type, acc, line, startCol});
        } else if (isdigit(c)) {
            int startCol = column;
            std::string acc = "";
            while (isdigit(peek())) acc += advance();
            if (peek() == '.' && isdigit(peekNext())) {
                acc += advance();
                while (isdigit(peek())) acc += advance();
            }
            tokens.push_back({TOKEN_NUMBER, acc, line, startCol});
        } else if (c == '"') {
            int startCol = column; advance();
            std::string strVal = "";
            while (peek() != '"' && !isAtEnd()) {
                if (peek() == '\n') { line++; column = 1; }
                strVal += advance();
            }
            if (isAtEnd()) {
                tokens.push_back({TOKEN_ERROR, strVal, line, startCol});
            } else {
                advance();
                tokens.push_back({TOKEN_STRING, strVal, line, startCol});
            }
        } else if (c == '=') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({OPERATOR_EQUAL, "==", line, startCol}); }
            else { tokens.push_back({OPERATOR_ASSIGN, "=", line, startCol}); }
        } else if (c == '!') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({OPERATOR_NOT_EQUAL, "!=", line, startCol}); }
            else { tokens.push_back({TOKEN_ERROR, "!", line, startCol}); }
        } else if (c == '<') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({OPERATOR_LESS_EQ, "<=", line, startCol}); }
            else { tokens.push_back({OPERATOR_LESS, "<", line, startCol}); }
        } else if (c == '>') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({OPERATOR_GREATER_EQ, ">=", line, startCol}); }
            else { tokens.push_back({OPERATOR_GREATER, ">", line, startCol}); }
        } else if (c == '+') {
            int startCol = column; advance(); tokens.push_back({OPERATOR_PLUS, "+", line, startCol});
        } else if (c == '-') {
            int startCol = column; advance(); tokens.push_back({OPERATOR_MINUS, "-", line, startCol});
        } else if (c == '*') {
            int startCol = column; advance(); tokens.push_back({OPERATOR_STAR, "*", line, startCol});
        } else if (c == ';') {
            int startCol = column; advance(); tokens.push_back({PUNCT_SEMICOLON, ";", line, startCol});
        } else if (c == '(') {
            int startCol = column; advance(); tokens.push_back({PUNCT_LPAREN, "(", line, startCol});
        } else if (c == ')') {
            int startCol = column; advance(); tokens.push_back({PUNCT_RPAREN, ")", line, startCol});
        } else if (c == '{') {
            int startCol = column; advance(); tokens.push_back({PUNCT_LBRACE, "{", line, startCol});
        } else if (c == '}') {
            int startCol = column; advance(); tokens.push_back({PUNCT_RBRACE, "}", line, startCol});
        } else {
            int startCol = column;
            std::string errStr(1, advance());
            tokens.push_back({TOKEN_ERROR, errStr, line, startCol});
        }
    }

    tokens.push_back({TOKEN_EOF, "", line, column});
    return tokens;
}
