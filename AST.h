#ifndef AST_H
#define AST_H
#include <stdbool.h>
#include <stddef.h>

// token
typedef enum {
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_MINUS, TOKEN_PLUS, TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,

    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,

    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

    TOKEN_CLASS, TOKEN_FUN, TOKEN_VAR, TOKEN_FOR, TOKEN_IF,
    TOKEN_WHILE, TOKEN_PRINT, TOKEN_RETURN,

    TOKEN_TRUE, TOKEN_FALSE, TOKEN_NIL,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* lexeme;
    int line;
} Token;

// source code value
typedef enum {
    LIT_NIL,
    LIT_BOOL,
    LIT_NUMBER,
    LIT_STRING
} LiteralType;

typedef struct {
    LiteralType type;
    union {
        bool boolean;
        double number;
        char* string;
    } as;
} LiteralValue;

// Expression (12 -> 5)
typedef enum {
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LITERAL,
    EXPR_GROUPING,
    EXPR_VARIABLE
} ExprType;

typedef struct Expr Expr;

struct Expr {
    ExprType type;
    union {
        struct {
            Expr* left;
            Token op;
            Expr* right;
        } binary;

        struct {
            Token op;
            Expr* right;
        } unary;

        struct {
            LiteralValue value;
        } literal;

        // parentheses ()
        struct {
            Expr* expression;
        } grouping;

        struct {
            Token name;
        } variable;
    } as;
};

// statement (9 -> 3)
typedef enum {
    STMT_EXPRESSION,
    STMT_PRINT,
    STMT_VAR
} StmtType;

typedef struct Stmt Stmt;

struct Stmt {
    StmtType type;
    union {
        struct {
            Expr* expression;
        } expression;

        struct {
            Expr* expression;
        } print;

        struct {
            Token name;
            Expr* initializer; // Có thể là NULL nếu không khởi tạo
        } var;
    } as;
};

Expr* new_binary_expr(Expr* left, Token op, Expr* right);
Expr* new_unary_expr(Token op, Expr* right);
Expr* new_literal_bool(bool value);
Expr* new_literal_number(double value);
Expr* new_literal_string(const char* str, size_t len);
Expr* new_literal_nil(void);
Expr* new_grouping_expr(Expr* expression);
Expr* new_variable_expr(Token name);

Stmt* new_expression_stmt(Expr* expression);
Stmt* new_print_stmt(Expr* expression);
Stmt* new_var_stmt(Token name, Expr* initializer);

// clear memory
void free_expr(Expr* expr);
void free_stmt(Stmt* stmt);

#endif