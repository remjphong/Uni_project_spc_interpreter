#include "Parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Expr* new_binary_expr(Expr* left, Token op, Expr* right) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_BINARY;
    expr->as.binary.left = left;
    expr->as.binary.op = op;
    expr->as.binary.right = right;
    return expr;
}

Expr* new_unary_expr(Token op, Expr* right) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_UNARY;
    expr->as.unary.op = op;
    expr->as.unary.right = right;
    return expr;
}

// literals
Expr* new_literal_bool(bool value) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->as.literal.value.type = LIT_BOOL;
    expr->as.literal.value.as.boolean = value;
    return expr;
}

Expr* new_literal_number(double value) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->as.literal.value.type = LIT_NUMBER;
    expr->as.literal.value.as.number = value;
    return expr;
}

Expr* new_literal_string(const char* str, size_t len) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->as.literal.value.type = LIT_STRING;

    // create a copy (token memory -> ast memory)
    char* copy = (char*)malloc(len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0'; // end of token

    expr->as.literal.value.as.string = copy;
    return expr;
}

// null value
Expr* new_literal_nil(void) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->as.literal.value.type = LIT_NIL;
    return expr;
}

// parentheses
Expr* new_grouping_expr(Expr* expression) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_GROUPING;
    expr->as.grouping.expression = expression;
    return expr;
}

// variable
Expr* new_variable_expr(Token name) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    expr->type = EXPR_VARIABLE;
    expr->as.variable.name = name;
    return expr;
}

// statements
Stmt* new_expression_stmt(Expr* expression) {
    Stmt* stmt = (Stmt*)malloc(sizeof(Stmt));
    stmt->type = STMT_EXPRESSION;
    stmt->as.expression.expression = expression;
    return stmt;
}

Stmt* new_print_stmt(Expr* expression) {
    Stmt* stmt = (Stmt*)malloc(sizeof(Stmt));
    stmt->type = STMT_PRINT;
    stmt->as.print.expression = expression;
    return stmt;
}

// ex: var x = 10
Stmt* new_var_stmt(Token name, Expr* initializer) {
    Stmt* stmt = (Stmt*)malloc(sizeof(Stmt));
    stmt->type = STMT_VAR;
    stmt->as.var.name = name;
    stmt->as.var.initializer = initializer;
    return stmt;
}

// free up memory
void free_expr(Expr* expr) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_BINARY:
            free_expr(expr->as.binary.left);
            free_expr(expr->as.binary.right);
            break;
        case EXPR_UNARY:
            free_expr(expr->as.unary.right);
            break;
        case EXPR_GROUPING:
            free_expr(expr->as.grouping.expression);
            break;
        case EXPR_LITERAL:
            if (expr->as.literal.value.type == LIT_STRING) {
                free(expr->as.literal.value.as.string);
            }
            break;
        case EXPR_VARIABLE:
            break;
    }
    free(expr);
}

void free_stmt(Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_EXPRESSION:
            free_expr(stmt->as.expression.expression);
            break;
        case STMT_PRINT:
            free_expr(stmt->as.print.expression);
            break;
        case STMT_VAR:
            free_expr(stmt->as.var.initializer);
            break;
    }
    free(stmt);
}

// dynamic array management
static void stmt_list_push(StmtList* list, Stmt* stmt) {
    // extend memory
    if (list->count >= list->capacity) {
        list->capacity = (list->capacity == 0) ? 8 : list->capacity * 2;

        list->items = (Stmt**)realloc(list->items, list->capacity * sizeof(Stmt*));
    }
    // add
    list->items[list->count++] = stmt;
}

// free up list stmt
void stmt_list_free(StmtList* list) {
    for (size_t i = 0; i < list->count; ++i) {
        free_stmt(list->items[i]);
    }
    
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

// function for navigation, check Token (Private -> static)
static const Token* peek(const Parser* p) {
    return &p->tokens[p->current];
}

static const Token* previous(const Parser* p) {
    return &p->tokens[p->current - 1];
}

static bool is_at_end(const Parser* p) {
    return peek(p)->type == TOKEN_EOF;
}

static const Token* advance(Parser* p) {
    if (!is_at_end(p)) {
        p->current++;
    }
    return previous(p);
}

static bool check(const Parser* p, TokenType type) {
    if (is_at_end(p)) return false;
    return peek(p)->type == type;
}

static bool match_one(Parser* p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

// extend match_one (check list)
static bool match_any(Parser* p, const TokenType* types, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (check(p, types[i])) {
            advance(p);
            return true;
        }
    }
    return false;
}

// syntax error handling and Parser recovery
static void error_at(Parser* p, const Token* token, const char* message) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    p->had_error = true;

    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else {
        fprintf(stderr, " at '%s'", token->lexeme);
    }
    fprintf(stderr, ": %s\n", message);
}

static const Token* consume(Parser* p, TokenType type, const char* message) {
    if (check(p, type)) {
        return advance(p);
    }
    error_at(p, peek(p), message);
    return NULL;
}

static void synchronize(Parser* p) {
    p->panic_mode = false;

    while (!is_at_end(p)) {
        if (p->current > 0 && previous(p)->type == TOKEN_SEMICOLON) {
            return;
        }

        switch (peek(p)->type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
            case TOKEN_RIGHT_BRACE:
                return;
            default:
                advance(p);
        }
    }
}

static Expr* expression(Parser* p);
static Stmt* statement(Parser* p);
static Stmt* declaration(Parser* p);

// expression analysis
static Expr* primary(Parser* p) {
    if (match_one(p, TOKEN_FALSE)) return new_literal_bool(false);
    if (match_one(p, TOKEN_TRUE))  return new_literal_bool(true);
    if (match_one(p, TOKEN_NIL))   return new_literal_nil();

    if (match_one(p, TOKEN_NUMBER)) {
        double val = strtod(previous(p)->lexeme, NULL);
        return new_literal_number(val);
    }

    if (match_one(p, TOKEN_STRING)) {
        const char* lex = previous(p)->lexeme;
        size_t len = strlen(lex);
        // Cắt bỏ cặp dấu ngoặc kép ở 2 đầu nếu có
        if (len >= 2 && lex[0] == '"' && lex[len - 1] == '"') {
            return new_literal_string(lex + 1, len - 2);
        }
        return new_literal_string(lex, len);
    }

    if (match_one(p, TOKEN_IDENTIFIER)) {
        return new_variable_expr(*previous(p));
    }

    if (match_one(p, TOKEN_LEFT_PAREN)) {
        Expr* expr = expression(p);
        if (!expr) return NULL;

        if (!consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.")) {
            free_expr(expr);
            return NULL;
        }
        return new_grouping_expr(expr);
    }

    error_at(p, peek(p), "Expect expression.");
    return NULL;
}

static Expr* unary(Parser* p) {
    const TokenType ops[] = {TOKEN_BANG, TOKEN_MINUS};
    if (match_any(p, ops, 2)) {
        Token op = *previous(p);
        Expr* right = unary(p);
        if (!right) return NULL;
        return new_unary_expr(op, right);
    }
    return primary(p);
}

static Expr* factor(Parser* p) {
    Expr* expr = unary(p);
    if (!expr) return NULL;

    const TokenType ops[] = {TOKEN_SLASH, TOKEN_STAR};
    while (match_any(p, ops, 2)) {
        Token op = *previous(p);
        Expr* right = unary(p);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(expr, op, right);
    }
    return expr;
}

static Expr* term(Parser* p) {
    Expr* expr = factor(p);
    if (!expr) return NULL;

    const TokenType ops[] = {TOKEN_MINUS, TOKEN_PLUS};
    while (match_any(p, ops, 2)) {
        Token op = *previous(p);
        Expr* right = factor(p);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(expr, op, right);
    }
    return expr;
}

static Expr* comparison(Parser* p) {
    Expr* expr = term(p);
    if (!expr) return NULL;

    const TokenType ops[] = {
        TOKEN_GREATER, TOKEN_GREATER_EQUAL,
        TOKEN_LESS, TOKEN_LESS_EQUAL
    };
    while (match_any(p, ops, 4)) {
        Token op = *previous(p);
        Expr* right = term(p);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(expr, op, right);
    }
    return expr;
}

static Expr* equality(Parser* p) {
    Expr* expr = comparison(p);
    if (!expr) return NULL;

    const TokenType ops[] = {TOKEN_BANG_EQUAL, TOKEN_EQUAL_EQUAL};
    while (match_any(p, ops, 2)) {
        Token op = *previous(p);
        Expr* right = comparison(p);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(expr, op, right);
    }
    return expr;
}

static Expr* expression(Parser* p) {
    return equality(p);
}

// Statements & Declarations analysis
static Stmt* print_statement(Parser* p) {
    Expr* value = expression(p);
    if (!value) return NULL;

    if (!consume(p, TOKEN_SEMICOLON, "Expect ';' after value.")) {
        free_expr(value);
        return NULL;
    }
    return new_print_stmt(value);
}

static Stmt* statement(Parser* p) {
    if (match_one(p, TOKEN_PRINT)) {
        return print_statement(p);
    }

    Expr* expr = expression(p);
    if (!expr) return NULL;

    if (!consume(p, TOKEN_SEMICOLON, "Expect ';' after expression.")) {
        free_expr(expr);
        return NULL;
    }
    return new_expression_stmt(expr);
}

static Stmt* var_declaration(Parser* p) {
    const Token* name = consume(p, TOKEN_IDENTIFIER, "Expect variable name.");
    if (!name) return NULL;

    Expr* initializer = NULL;
    if (match_one(p, TOKEN_EQUAL)) {
        initializer = expression(p);
        if (!initializer) return NULL;
    }

    if (!consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration.")) {
        free_expr(initializer);
        return NULL;
    }
    return new_var_stmt(*name, initializer);
}

static Stmt* declaration(Parser* p) {
    Stmt* stmt = NULL;

    if (match_one(p, TOKEN_VAR)) {
        stmt = var_declaration(p);
    } else {
        stmt = statement(p);
    }

    if (p->panic_mode) {
        synchronize(p);
        return NULL;
    }
    return stmt;
}

// parser interface
void parser_init(Parser* parser, const Token* tokens, size_t token_count) {
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current = 0;
    parser->had_error = false;
    parser->panic_mode = false;
}

StmtList parser_parse(Parser* parser) {
    StmtList statements = {NULL, 0, 0};

    while (!is_at_end(parser)) {
        Stmt* stmt = declaration(parser);
        if (stmt) {
            stmt_list_push(&statements, stmt);
        }
    }
    return statements;
}
