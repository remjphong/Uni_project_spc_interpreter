#ifndef TOKEN_H
#define TOKEN_H

#include <iostream>
#include <string>

enum TokenType {
    KW_VAR, KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_RETURN,
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING,
    OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_STAR, OPERATOR_SLASH,
    OPERATOR_ASSIGN, OPERATOR_EQUAL, OPERATOR_NOT_EQUAL,
    OPERATOR_LESS, OPERATOR_LESS_EQ, OPERATOR_GREATER, OPERATOR_GREATER_EQ,
    PUNCT_SEMICOLON, PUNCT_LPAREN, PUNCT_RPAREN, PUNCT_LBRACE, PUNCT_RBRACE,
    TOKEN_EOF, TOKEN_ERROR
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    void print() const {
        std::cout << "Token [Type: " << type 
                  << ", Lexeme: '" << lexeme << "'"
                  << ", Line: " << line 
                  << ", Col: " << column << "]\n";
    }
};

#endif // TOKEN_H
