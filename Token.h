#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>

/* All token types, preserving the original order */
typedef enum {
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    COMMA,
    DOT,
    MINUS,
    PLUS,
    SEMICOLON,
    SLASH,
    STAR,

    BANG,
    BANG_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,

    IDENTIFIER,
    STRING,
    NUMBER,

    AND,
    CLASS,
    ELSE,
    FALSE,
    FUN,
    FOR,
    IF,
    NIL,
    OR,
    PRINT,
    RETURN,
    SUPER,
    THIS,
    TRUE,
    VAR,
    WHILE,

    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

/*
 * lexeme is dynamically allocated.
 * The token owns its lexeme until it is freed.
 *
 * STRING lexemes exclude the surrounding quotation marks,
 * preserving the original lexer behavior.
 */
typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

/* Convert a token type to a readable name */
static inline const char *tokenTypeToString(TokenType type)
{
    switch (type) {
        case LEFT_PAREN:    return "LEFT_PAREN";
        case RIGHT_PAREN:   return "RIGHT_PAREN";
        case LEFT_BRACE:    return "LEFT_BRACE";
        case RIGHT_BRACE:   return "RIGHT_BRACE";
        case COMMA:         return "COMMA";
        case DOT:           return "DOT";
        case MINUS:         return "MINUS";
        case PLUS:          return "PLUS";
        case SEMICOLON:     return "SEMICOLON";
        case SLASH:         return "SLASH";
        case STAR:          return "STAR";

        case BANG:          return "BANG";
        case BANG_EQUAL:    return "BANG_EQUAL";
        case EQUAL:         return "EQUAL";
        case EQUAL_EQUAL:   return "EQUAL_EQUAL";
        case GREATER:       return "GREATER";
        case GREATER_EQUAL: return "GREATER_EQUAL";
        case LESS:          return "LESS";
        case LESS_EQUAL:    return "LESS_EQUAL";

        case IDENTIFIER:    return "IDENTIFIER";
        case STRING:        return "STRING";
        case NUMBER:        return "NUMBER";

        case AND:           return "AND";
        case CLASS:         return "CLASS";
        case ELSE:          return "ELSE";
        case FALSE:         return "FALSE";
        case FUN:           return "FUN";
        case FOR:           return "FOR";
        case IF:            return "IF";
        case NIL:           return "NIL";
        case OR:            return "OR";
        case PRINT:         return "PRINT";
        case RETURN:        return "RETURN";
        case SUPER:         return "SUPER";
        case THIS:          return "THIS";
        case TRUE:          return "TRUE";
        case VAR:           return "VAR";
        case WHILE:         return "WHILE";

        case TOKEN_EOF:     return "EOF";
        case TOKEN_ERROR:   return "ERROR";
        default:            return "UNKNOWN";
    }
}

/* Print one token; useful for debugging and testing */
static inline void printToken(const Token *token)
{
    if (token == NULL) {
        return;
    }

    printf(
        "Token [%s, Lexeme: '%s', Line: %d, Col: %d]\n",
        tokenTypeToString(token->type),
        token->lexeme != NULL ? token->lexeme : "",
        token->line,
        token->column
    );
}

#endif /* TOKEN_H */
