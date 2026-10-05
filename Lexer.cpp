#include "Lexer.h"
#include <cctype>
#include <iostream>
#include <utility>

Lexer::Lexer(std::string src) : source(std::move(src)) {
    keywords = {
        {"and", AND},
        {"class", CLASS},
        {"else", ELSE},
        {"false", FALSE},
        {"for", FOR},
        {"fun", FUN},
        {"if", IF},
        {"nil", NIL},
        {"or", OR},
        {"print", PRINT},
        {"return", RETURN},
        {"super", SUPER},
        {"this", THIS},
        {"true", TRUE},
        {"var", VAR},
        {"while", WHILE}
    };
}

bool Lexer::isAtEnd() const {
    return cursor >= source.length();
}

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

        // 1. Whitespace & Newline handling
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            line++;
            advance();
            column = 1; // Reset column ve 1 khi xuong dong mới
        } 
        
        // 2. Comments & SLASH operator
        else if (c == '/') {
            if (peekNext() == '/') {
                // Single line comment
                while (peek() != '\n' && !isAtEnd()) advance();
            } else if (peekNext() == '*') {
                // Multiline block comment
                int startLine = line;
                int startCol = column;
                advance(); advance(); // An /*

                while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                    if (peek() == '\n') { 
                        line++; 
                        advance(); 
                        column = 1; 
                    } else {
                        advance();
                    }
                }

                if (isAtEnd()) {
                    tokens.push_back({TOKEN_ERROR, "Unterminated block comment", startLine, startCol});
                } else {
                    advance(); advance(); // An */
                }
            } else {
                int startCol = column; advance();
                tokens.push_back({SLASH, "/", line, startCol});
            }
        } 

        // 3. Single / Double character operators & Punctuators
        else if (c == '(') { int startCol = column; advance(); tokens.push_back({LEFT_PAREN, "(", line, startCol}); }
        else if (c == ')') { int startCol = column; advance(); tokens.push_back({RIGHT_PAREN, ")", line, startCol}); }
        else if (c == '{') { int startCol = column; advance(); tokens.push_back({LEFT_BRACE, "{", line, startCol}); }
        else if (c == '}') { int startCol = column; advance(); tokens.push_back({RIGHT_BRACE, "}", line, startCol}); }
        else if (c == ',') { int startCol = column; advance(); tokens.push_back({COMMA, ",", line, startCol}); }
        else if (c == '.') { int startCol = column; advance(); tokens.push_back({DOT, ".", line, startCol}); }
        else if (c == '-') { int startCol = column; advance(); tokens.push_back({MINUS, "-", line, startCol}); }
        else if (c == '+') { int startCol = column; advance(); tokens.push_back({PLUS, "+", line, startCol}); }
        else if (c == ';') { int startCol = column; advance(); tokens.push_back({SEMICOLON, ";", line, startCol}); }
        else if (c == '*') { int startCol = column; advance(); tokens.push_back({STAR, "*", line, startCol}); }
        
        else if (c == '!') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({BANG_EQUAL, "!=", line, startCol}); }
            else { tokens.push_back({BANG, "!", line, startCol}); }
        } else if (c == '=') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({EQUAL_EQUAL, "==", line, startCol}); }
            else { tokens.push_back({EQUAL, "=", line, startCol}); }
        } else if (c == '<') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({LESS_EQUAL, "<=", line, startCol}); }
            else { tokens.push_back({LESS, "<", line, startCol}); }
        } else if (c == '>') {
            int startCol = column; advance();
            if (peek() == '=') { advance(); tokens.push_back({GREATER_EQUAL, ">=", line, startCol}); }
            else { tokens.push_back({GREATER, ">", line, startCol}); }
        }

        // 4. Identifiers & Keywords (Dung static_cast<unsigned char> an toan)
        else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            int startCol = column;
            std::string acc = "";
            while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
                acc += advance();
            }

            TokenType type = keywords.count(acc) ? keywords[acc] : IDENTIFIER;
            tokens.push_back({type, acc, line, startCol});
        }

        // 5. Numbers
        else if (std::isdigit(static_cast<unsigned char>(c))) {
            int startCol = column;
            std::string acc = "";
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                acc += advance();
            }

            if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
                acc += advance();
                while (std::isdigit(static_cast<unsigned char>(peek()))) {
                    acc += advance();
                }
            }
            tokens.push_back({NUMBER, acc, line, startCol});
        }

        // 6. String Literals
        else if (c == '"') {
            int startLine = line;
            int startCol = column;
            advance(); // An dấu " mở
            std::string strVal = "";

            while (peek() != '"' && !isAtEnd()) {
                if (peek() == '\n') { 
                    line++; 
                    strVal += advance();
                    column = 1; 
                } else {
                    strVal += advance();
                }
            }

            if (isAtEnd()) {
                tokens.push_back({TOKEN_ERROR, "Unterminated string", startLine, startCol});
            } else {
                advance(); // An dấu " đóng
                tokens.push_back({STRING, strVal, startLine, startCol});
            }
        }

        // 7. Ky tu loi
        else {
            int startCol = column;
            std::string errStr(1, advance());
            tokens.push_back({TOKEN_ERROR, "Unexpected character: " + errStr, line, startCol});
        }
    }

    tokens.push_back({TOKEN_EOF, "", line, column});
    return tokens;
}