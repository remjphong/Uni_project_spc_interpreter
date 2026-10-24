#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>

#include "token.h"

/* Literal values supported by the interpreter. */
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
        char *string;
    } as;
} LiteralValue;

typedef enum {
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LITERAL,
    EXPR_GROUPING,
    EXPR_VARIABLE,
    EXPR_ASSIGN,
    EXPR_LOGICAL,
    EXPR_CALL,
    EXPR_GET,
    EXPR_SET,
    EXPR_THIS,
    EXPR_SUPER
} ExprType;

typedef enum {
    STMT_EXPRESSION,
    STMT_PRINT,
    STMT_VAR,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE,
    STMT_FUNCTION,
    STMT_RETURN,
    STMT_CLASS
} StmtType;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    Expr **items;
    size_t count;
    size_t capacity;
} ExprList;

typedef struct StmtList {
    Stmt **items;
    size_t count;
    size_t capacity;
} StmtList;

struct Expr {
    ExprType type;
    union {
        struct {
            Expr *left;
            Token op;
            Expr *right;
        } binary;
        struct {
            Token op;
            Expr *right;
        } unary;
        struct {
            LiteralValue value;
        } literal;
        struct {
            Expr *expression;
        } grouping;
        struct {
            Token name;
        } variable;
        struct {
            Token name;
            Expr *value;
        } assign;
        struct {
            Expr *left;
            Token op;
            Expr *right;
        } logical;
        struct {
            Expr *callee;
            Token paren;
            ExprList arguments;
        } call;
        struct {
            Expr *object;
            Token name;
        } get;
        struct {
            Expr *object;
            Token name;
            Expr *value;
        } set;
        struct {
            Token keyword;
        } this_expr;
        struct {
            Token keyword;
            Token method;
        } super_expr;
    } as;
};

struct Stmt {
    StmtType type;
    union {
        struct {
            Expr *expression;
        } expression;
        struct {
            Expr *expression;
        } print;
        struct {
            Token name;
            Expr *initializer;
        } var;
        struct {
            StmtList statements;
        } block;
        struct {
            Expr *condition;
            Stmt *then_branch;
            Stmt *else_branch;
        } if_stmt;
        struct {
            Expr *condition;
            Stmt *body;
        } while_stmt;
        struct {
            Token name;
            Token *params;
            size_t param_count;
            StmtList body;
        } function;
        struct {
            Token keyword;
            Expr *value;
        } return_stmt;
        struct {
            Token name;
            Expr *superclass; /* NULL or an EXPR_VARIABLE node. */
            StmtList methods; /* Every item is a STMT_FUNCTION node. */
        } class_stmt;
    } as;
};

/*
 * Ownership contract:
 * - Every Token stored inside an AST node owns a copied lexeme.
 * - AST nodes own their child Expr/Stmt pointers and list contents.
 * - TokenArray remains independently owned by the lexer and may be freed
 *   after parser_parse() returns.
 */
void free_expr(Expr *expr);
void free_stmt(Stmt *stmt);
void stmt_list_free(StmtList *list);
void expr_list_free(ExprList *list);

/* Constructors retained from the existing C parser API. */
Expr *new_binary_expr(Expr *left, Token op, Expr *right);
Expr *new_unary_expr(Token op, Expr *right);
Expr *new_literal_bool(bool value);
Expr *new_literal_number(double value);
Expr *new_literal_string(const char *str, size_t len);
Expr *new_literal_nil(void);
Expr *new_grouping_expr(Expr *expression);
Expr *new_variable_expr(Token name);

Stmt *new_expression_stmt(Expr *expression);
Stmt *new_print_stmt(Expr *expression);
Stmt *new_var_stmt(Token name, Expr *initializer);

#endif /* AST_H */
