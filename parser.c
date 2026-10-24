#include "parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Token ownership helpers
 * ================================================================ */

static void token_clear(Token *token)
{
    if (token == NULL) {
        return;
    }

    free(token->lexeme);
    token->lexeme = NULL;
}

static int token_copy(Token *destination, const Token *source)
{
    size_t length;
    char *copy;

    if (destination == NULL || source == NULL) {
        return 0;
    }

    destination->type = source->type;
    destination->line = source->line;
    destination->column = source->column;
    destination->lexeme = NULL;

    length = source->lexeme != NULL ? strlen(source->lexeme) : 0;
    if (length == SIZE_MAX) {
        return 0;
    }

    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return 0;
    }

    if (length != 0 && source->lexeme != NULL) {
        memcpy(copy, source->lexeme, length);
    }
    copy[length] = '\0';

    destination->lexeme = copy;
    return 1;
}

static void token_array_free(Token *tokens, size_t count)
{
    size_t i;

    if (tokens == NULL) {
        return;
    }

    for (i = 0; i < count; ++i) {
        token_clear(&tokens[i]);
    }
    free(tokens);
}

/* ================================================================
 * AST list and memory management
 * ================================================================ */

static int stmt_list_push(StmtList *list, Stmt *stmt)
{
    Stmt **new_items;
    size_t new_capacity;

    if (list == NULL || stmt == NULL) {
        return 0;
    }

    if (list->count == list->capacity) {
        if (list->capacity == 0) {
            new_capacity = 8;
        } else {
            if (list->capacity > SIZE_MAX / 2) {
                return 0;
            }
            new_capacity = list->capacity * 2;
        }

        if (new_capacity > SIZE_MAX / sizeof(*list->items)) {
            return 0;
        }

        new_items = (Stmt **)realloc(
            list->items,
            new_capacity * sizeof(*list->items)
        );
        if (new_items == NULL) {
            return 0;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count++] = stmt;
    return 1;
}

static int expr_list_push(ExprList *list, Expr *expr)
{
    Expr **new_items;
    size_t new_capacity;

    if (list == NULL || expr == NULL) {
        return 0;
    }

    if (list->count == list->capacity) {
        if (list->capacity == 0) {
            new_capacity = 8;
        } else {
            if (list->capacity > SIZE_MAX / 2) {
                return 0;
            }
            new_capacity = list->capacity * 2;
        }

        if (new_capacity > SIZE_MAX / sizeof(*list->items)) {
            return 0;
        }

        new_items = (Expr **)realloc(
            list->items,
            new_capacity * sizeof(*list->items)
        );
        if (new_items == NULL) {
            return 0;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count++] = expr;
    return 1;
}

void expr_list_free(ExprList *list)
{
    size_t i;

    if (list == NULL) {
        return;
    }

    for (i = 0; i < list->count; ++i) {
        free_expr(list->items[i]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void stmt_list_free(StmtList *list)
{
    size_t i;

    if (list == NULL) {
        return;
    }

    for (i = 0; i < list->count; ++i) {
        free_stmt(list->items[i]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void free_expr(Expr *expr)
{
    if (expr == NULL) {
        return;
    }

    switch (expr->type) {
        case EXPR_BINARY:
            free_expr(expr->as.binary.left);
            token_clear(&expr->as.binary.op);
            free_expr(expr->as.binary.right);
            break;

        case EXPR_UNARY:
            token_clear(&expr->as.unary.op);
            free_expr(expr->as.unary.right);
            break;

        case EXPR_LITERAL:
            if (expr->as.literal.value.type == LIT_STRING) {
                free(expr->as.literal.value.as.string);
                expr->as.literal.value.as.string = NULL;
            }
            break;

        case EXPR_GROUPING:
            free_expr(expr->as.grouping.expression);
            break;

        case EXPR_VARIABLE:
            token_clear(&expr->as.variable.name);
            break;

        case EXPR_ASSIGN:
            token_clear(&expr->as.assign.name);
            free_expr(expr->as.assign.value);
            break;

        case EXPR_LOGICAL:
            free_expr(expr->as.logical.left);
            token_clear(&expr->as.logical.op);
            free_expr(expr->as.logical.right);
            break;

        case EXPR_CALL:
            free_expr(expr->as.call.callee);
            token_clear(&expr->as.call.paren);
            expr_list_free(&expr->as.call.arguments);
            break;

        case EXPR_GET:
            free_expr(expr->as.get.object);
            token_clear(&expr->as.get.name);
            break;

        case EXPR_SET:
            free_expr(expr->as.set.object);
            token_clear(&expr->as.set.name);
            free_expr(expr->as.set.value);
            break;

        case EXPR_THIS:
            token_clear(&expr->as.this_expr.keyword);
            break;

        case EXPR_SUPER:
            token_clear(&expr->as.super_expr.keyword);
            token_clear(&expr->as.super_expr.method);
            break;
    }

    free(expr);
}

void free_stmt(Stmt *stmt)
{
    if (stmt == NULL) {
        return;
    }

    switch (stmt->type) {
        case STMT_EXPRESSION:
            free_expr(stmt->as.expression.expression);
            break;

        case STMT_PRINT:
            free_expr(stmt->as.print.expression);
            break;

        case STMT_VAR:
            token_clear(&stmt->as.var.name);
            free_expr(stmt->as.var.initializer);
            break;

        case STMT_BLOCK:
            stmt_list_free(&stmt->as.block.statements);
            break;

        case STMT_IF:
            free_expr(stmt->as.if_stmt.condition);
            free_stmt(stmt->as.if_stmt.then_branch);
            free_stmt(stmt->as.if_stmt.else_branch);
            break;

        case STMT_WHILE:
            free_expr(stmt->as.while_stmt.condition);
            free_stmt(stmt->as.while_stmt.body);
            break;

        case STMT_FUNCTION:
            token_clear(&stmt->as.function.name);
            token_array_free(stmt->as.function.params,
                             stmt->as.function.param_count);
            stmt_list_free(&stmt->as.function.body);
            break;

        case STMT_RETURN:
            token_clear(&stmt->as.return_stmt.keyword);
            free_expr(stmt->as.return_stmt.value);
            break;

        case STMT_CLASS:
            token_clear(&stmt->as.class_stmt.name);
            free_expr(stmt->as.class_stmt.superclass);
            stmt_list_free(&stmt->as.class_stmt.methods);
            break;
    }

    free(stmt);
}

/* ================================================================
 * AST constructors
 * Constructors deep-copy Token lexemes and take child pointers only
 * after all operations that can fail have succeeded.
 * ================================================================ */

static Expr *new_expr(ExprType type)
{
    Expr *expr = (Expr *)calloc(1, sizeof(*expr));
    if (expr != NULL) {
        expr->type = type;
    }
    return expr;
}

static Stmt *new_stmt(StmtType type)
{
    Stmt *stmt = (Stmt *)calloc(1, sizeof(*stmt));
    if (stmt != NULL) {
        stmt->type = type;
    }
    return stmt;
}

Expr *new_binary_expr(Expr *left, Token op, Expr *right)
{
    Expr *expr = new_expr(EXPR_BINARY);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.binary.op, &op)) {
        free(expr);
        return NULL;
    }
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    return expr;
}

Expr *new_unary_expr(Token op, Expr *right)
{
    Expr *expr = new_expr(EXPR_UNARY);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.unary.op, &op)) {
        free(expr);
        return NULL;
    }
    expr->as.unary.right = right;
    return expr;
}

Expr *new_literal_bool(bool value)
{
    Expr *expr = new_expr(EXPR_LITERAL);
    if (expr == NULL) {
        return NULL;
    }
    expr->as.literal.value.type = LIT_BOOL;
    expr->as.literal.value.as.boolean = value;
    return expr;
}

Expr *new_literal_number(double value)
{
    Expr *expr = new_expr(EXPR_LITERAL);
    if (expr == NULL) {
        return NULL;
    }
    expr->as.literal.value.type = LIT_NUMBER;
    expr->as.literal.value.as.number = value;
    return expr;
}

Expr *new_literal_string(const char *str, size_t len)
{
    Expr *expr;
    char *copy;

    if (str == NULL && len != 0) {
        return NULL;
    }
    if (len == SIZE_MAX) {
        return NULL;
    }

    expr = new_expr(EXPR_LITERAL);
    if (expr == NULL) {
        return NULL;
    }

    copy = (char *)malloc(len + 1);
    if (copy == NULL) {
        free(expr);
        return NULL;
    }
    if (len != 0) {
        memcpy(copy, str, len);
    }
    copy[len] = '\0';

    expr->as.literal.value.type = LIT_STRING;
    expr->as.literal.value.as.string = copy;
    return expr;
}

Expr *new_literal_nil(void)
{
    Expr *expr = new_expr(EXPR_LITERAL);
    if (expr != NULL) {
        expr->as.literal.value.type = LIT_NIL;
    }
    return expr;
}

Expr *new_grouping_expr(Expr *expression)
{
    Expr *expr = new_expr(EXPR_GROUPING);
    if (expr != NULL) {
        expr->as.grouping.expression = expression;
    }
    return expr;
}

Expr *new_variable_expr(Token name)
{
    Expr *expr = new_expr(EXPR_VARIABLE);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.variable.name, &name)) {
        free(expr);
        return NULL;
    }
    return expr;
}

static Expr *new_assign_expr(Token name, Expr *value)
{
    Expr *expr = new_expr(EXPR_ASSIGN);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.assign.name, &name)) {
        free(expr);
        return NULL;
    }
    expr->as.assign.value = value;
    return expr;
}

static Expr *new_logical_expr(Expr *left, Token op, Expr *right)
{
    Expr *expr = new_expr(EXPR_LOGICAL);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.logical.op, &op)) {
        free(expr);
        return NULL;
    }
    expr->as.logical.left = left;
    expr->as.logical.right = right;
    return expr;
}

static Expr *new_call_expr(Expr *callee, Token paren, ExprList arguments)
{
    Expr *expr = new_expr(EXPR_CALL);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.call.paren, &paren)) {
        free(expr);
        return NULL;
    }
    expr->as.call.callee = callee;
    expr->as.call.arguments = arguments;
    return expr;
}

static Expr *new_get_expr(Expr *object, Token name)
{
    Expr *expr = new_expr(EXPR_GET);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.get.name, &name)) {
        free(expr);
        return NULL;
    }
    expr->as.get.object = object;
    return expr;
}

static Expr *new_set_expr(Expr *object, Token name, Expr *value)
{
    Expr *expr = new_expr(EXPR_SET);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.set.name, &name)) {
        free(expr);
        return NULL;
    }
    expr->as.set.object = object;
    expr->as.set.value = value;
    return expr;
}

static Expr *new_this_expr(Token keyword)
{
    Expr *expr = new_expr(EXPR_THIS);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.this_expr.keyword, &keyword)) {
        free(expr);
        return NULL;
    }
    return expr;
}

static Expr *new_super_expr(Token keyword, Token method)
{
    Expr *expr = new_expr(EXPR_SUPER);
    if (expr == NULL) {
        return NULL;
    }
    if (!token_copy(&expr->as.super_expr.keyword, &keyword)) {
        free(expr);
        return NULL;
    }
    if (!token_copy(&expr->as.super_expr.method, &method)) {
        token_clear(&expr->as.super_expr.keyword);
        free(expr);
        return NULL;
    }
    return expr;
}

Stmt *new_expression_stmt(Expr *expression)
{
    Stmt *stmt = new_stmt(STMT_EXPRESSION);
    if (stmt != NULL) {
        stmt->as.expression.expression = expression;
    }
    return stmt;
}

Stmt *new_print_stmt(Expr *expression)
{
    Stmt *stmt = new_stmt(STMT_PRINT);
    if (stmt != NULL) {
        stmt->as.print.expression = expression;
    }
    return stmt;
}

Stmt *new_var_stmt(Token name, Expr *initializer)
{
    Stmt *stmt = new_stmt(STMT_VAR);
    if (stmt == NULL) {
        return NULL;
    }
    if (!token_copy(&stmt->as.var.name, &name)) {
        free(stmt);
        return NULL;
    }
    stmt->as.var.initializer = initializer;
    return stmt;
}

static Stmt *new_block_stmt(StmtList statements)
{
    Stmt *stmt = new_stmt(STMT_BLOCK);
    if (stmt != NULL) {
        stmt->as.block.statements = statements;
    }
    return stmt;
}

static Stmt *new_if_stmt(Expr *condition, Stmt *then_branch,
                         Stmt *else_branch)
{
    Stmt *stmt = new_stmt(STMT_IF);
    if (stmt != NULL) {
        stmt->as.if_stmt.condition = condition;
        stmt->as.if_stmt.then_branch = then_branch;
        stmt->as.if_stmt.else_branch = else_branch;
    }
    return stmt;
}

static Stmt *new_while_stmt(Expr *condition, Stmt *body)
{
    Stmt *stmt = new_stmt(STMT_WHILE);
    if (stmt != NULL) {
        stmt->as.while_stmt.condition = condition;
        stmt->as.while_stmt.body = body;
    }
    return stmt;
}

static Stmt *new_function_stmt(Token name, Token *params,
                               size_t param_count, StmtList body)
{
    Stmt *stmt = new_stmt(STMT_FUNCTION);
    if (stmt == NULL) {
        return NULL;
    }
    if (!token_copy(&stmt->as.function.name, &name)) {
        free(stmt);
        return NULL;
    }
    stmt->as.function.params = params;
    stmt->as.function.param_count = param_count;
    stmt->as.function.body = body;
    return stmt;
}

static Stmt *new_return_stmt(Token keyword, Expr *value)
{
    Stmt *stmt = new_stmt(STMT_RETURN);
    if (stmt == NULL) {
        return NULL;
    }
    if (!token_copy(&stmt->as.return_stmt.keyword, &keyword)) {
        free(stmt);
        return NULL;
    }
    stmt->as.return_stmt.value = value;
    return stmt;
}

static Stmt *new_class_stmt(Token name, Expr *superclass, StmtList methods)
{
    Stmt *stmt = new_stmt(STMT_CLASS);
    if (stmt == NULL) {
        return NULL;
    }
    if (!token_copy(&stmt->as.class_stmt.name, &name)) {
        free(stmt);
        return NULL;
    }
    stmt->as.class_stmt.superclass = superclass;
    stmt->as.class_stmt.methods = methods;
    return stmt;
}

/* ================================================================
 * Parser navigation and error recovery
 * ================================================================ */

static const Token *peek(const Parser *parser)
{
    static char empty_lexeme[] = "";
    static const Token fallback_eof = {TOKEN_EOF, empty_lexeme, 1, 1};

    if (parser == NULL || parser->tokens == NULL ||
        parser->current >= parser->token_count) {
        return &fallback_eof;
    }
    return &parser->tokens[parser->current];
}

static const Token *previous(const Parser *parser)
{
    if (parser == NULL || parser->current == 0 || parser->tokens == NULL) {
        return peek(parser);
    }
    if (parser->current - 1 >= parser->token_count) {
        return peek(parser);
    }
    return &parser->tokens[parser->current - 1];
}

static bool is_at_end(const Parser *parser)
{
    return peek(parser)->type == TOKEN_EOF;
}

static const Token *advance(Parser *parser)
{
    if (!is_at_end(parser) && parser->current < parser->token_count) {
        ++parser->current;
    }
    return previous(parser);
}

/* Match the C++ check(): TOKEN_EOF must match at EOF. */
static bool check(const Parser *parser, TokenType type)
{
    return peek(parser)->type == type;
}

static bool match_one(Parser *parser, TokenType type)
{
    if (!check(parser, type)) {
        return false;
    }
    (void)advance(parser);
    return true;
}

static bool match_any(Parser *parser, const TokenType *types, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        if (check(parser, types[i])) {
            (void)advance(parser);
            return true;
        }
    }
    return false;
}

static void error_at(Parser *parser, const Token *token, const char *message)
{
    if (parser == NULL || token == NULL || message == NULL ||
        parser->panic_mode) {
        return;
    }

    parser->panic_mode = true;
    parser->had_error = true;

    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else {
        fprintf(stderr, " at '%s'", token->lexeme != NULL ? token->lexeme : "");
    }
    fprintf(stderr, ": %s\n", message);
}

static const Token *consume(Parser *parser, TokenType type,
                            const char *message)
{
    if (check(parser, type)) {
        return advance(parser);
    }

    error_at(parser, peek(parser), message);
    return NULL;
}

static void synchronize(Parser *parser)
{
    static const TokenType starters[] = {
        CLASS, FUN, VAR, FOR, IF, WHILE, PRINT, RETURN, RIGHT_BRACE
    };

    parser->panic_mode = false;

    if (parser->current > 0 && previous(parser)->type == SEMICOLON) {
        return;
    }

    while (!is_at_end(parser)) {
        size_t i;

        for (i = 0; i < sizeof(starters) / sizeof(starters[0]); ++i) {
            if (check(parser, starters[i])) {
                return;
            }
        }
        (void)advance(parser);
    }
}

static void mark_out_of_memory(Parser *parser)
{
    if (parser != NULL) {
        parser->out_of_memory = true;
        parser->had_error = true;
    }
}

/* ================================================================
 * Forward declarations for recursive-descent grammar
 * ================================================================ */

static Stmt *declaration(Parser *parser);
static Stmt *class_declaration(Parser *parser);
static Stmt *function_declaration(Parser *parser, const char *kind);
static Stmt *var_declaration(Parser *parser);
static Stmt *statement(Parser *parser);
static Stmt *for_statement(Parser *parser);
static Stmt *if_statement(Parser *parser);
static Stmt *print_statement(Parser *parser);
static Stmt *return_statement(Parser *parser);
static Stmt *while_statement(Parser *parser);
static bool block_contents(Parser *parser, StmtList *statements);

static Expr *expression(Parser *parser);
static Expr *assignment(Parser *parser);
static Expr *logic_or(Parser *parser);
static Expr *logic_and(Parser *parser);
static Expr *equality(Parser *parser);
static Expr *comparison(Parser *parser);
static Expr *term(Parser *parser);
static Expr *factor(Parser *parser);
static Expr *unary(Parser *parser);
static Expr *call(Parser *parser);
static Expr *finish_call(Parser *parser, Expr *callee);
static Expr *primary(Parser *parser);

/* ================================================================
 * Declarations
 * ================================================================ */

static Stmt *declaration(Parser *parser)
{
    Stmt *stmt = NULL;

    if (match_one(parser, CLASS)) {
        stmt = class_declaration(parser);
    } else if (match_one(parser, FUN)) {
        stmt = function_declaration(parser, "function");
    } else if (match_one(parser, VAR)) {
        stmt = var_declaration(parser);
    } else {
        stmt = statement(parser);
    }

    if (parser->out_of_memory) {
        free_stmt(stmt);
        return NULL;
    }

    if (parser->panic_mode) {
        free_stmt(stmt);
        synchronize(parser);
        return NULL;
    }

    return stmt;
}

static Stmt *class_declaration(Parser *parser)
{
    const Token *name = consume(parser, IDENTIFIER, "Expect class name.");
    Expr *superclass = NULL;
    StmtList methods = {0};
    Stmt *result;

    if (name == NULL) {
        return NULL;
    }

    if (match_one(parser, LESS)) {
        const Token *super_name = consume(
            parser, IDENTIFIER, "Expect superclass name.");
        if (super_name == NULL) {
            return NULL;
        }

        superclass = new_variable_expr(*super_name);
        if (superclass == NULL) {
            mark_out_of_memory(parser);
            return NULL;
        }
    }

    if (consume(parser, LEFT_BRACE,
                "Expect '{' before class body.") == NULL) {
        free_expr(superclass);
        return NULL;
    }

    while (!check(parser, RIGHT_BRACE) && !is_at_end(parser)) {
        Stmt *method = function_declaration(parser, "method");

        if (parser->out_of_memory || parser->panic_mode) {
            free_stmt(method);
            free_expr(superclass);
            stmt_list_free(&methods);
            return NULL;
        }

        if (method == NULL || method->type != STMT_FUNCTION) {
            free_stmt(method);
            mark_out_of_memory(parser);
            free_expr(superclass);
            stmt_list_free(&methods);
            return NULL;
        }

        if (!stmt_list_push(&methods, method)) {
            free_stmt(method);
            mark_out_of_memory(parser);
            free_expr(superclass);
            stmt_list_free(&methods);
            return NULL;
        }
    }

    if (consume(parser, RIGHT_BRACE,
                "Expect '}' after class body.") == NULL) {
        free_expr(superclass);
        stmt_list_free(&methods);
        return NULL;
    }

    result = new_class_stmt(*name, superclass, methods);
    if (result == NULL) {
        mark_out_of_memory(parser);
        free_expr(superclass);
        stmt_list_free(&methods);
    }
    return result;
}

static int parameter_push(Token **params,
                          size_t *count, size_t *capacity,
                          const Token *token)
{
    Token *new_params;
    size_t new_capacity;
    Token copied;

    if (token == NULL || params == NULL || count == NULL || capacity == NULL) {
        return 0;
    }

    if (*count == *capacity) {
        if (*capacity == 0) {
            new_capacity = 8;
        } else {
            if (*capacity > SIZE_MAX / 2) {
                return 0;
            }
            new_capacity = *capacity * 2;
        }
        if (new_capacity > SIZE_MAX / sizeof(**params)) {
            return 0;
        }

        new_params = (Token *)realloc(*params,
                                      new_capacity * sizeof(**params));
        if (new_params == NULL) {
            return 0;
        }
        *params = new_params;
        *capacity = new_capacity;
    }

    if (!token_copy(&copied, token)) {
        return 0;
    }

    (*params)[(*count)++] = copied;
    return 1;
}

static Stmt *function_declaration(Parser *parser, const char *kind)
{
    const Token *name;
    Token *params = NULL;
    size_t param_count = 0;
    size_t param_capacity = 0;
    StmtList body = {0};
    Stmt *result;

    char message[128];

    (void)snprintf(message, sizeof(message), "Expect %s name.", kind);
    name = consume(parser, IDENTIFIER, message);
    if (name == NULL) {
        return NULL;
    }

    if (consume(parser, LEFT_PAREN, "Expect '(' after name.") == NULL) {
        return NULL;
    }

    if (!check(parser, RIGHT_PAREN)) {
        do {
            const Token *param = consume(
                parser, IDENTIFIER, "Expect parameter name.");
            if (param == NULL) {
                token_array_free(params, param_count);
                return NULL;
            }
            if (!parameter_push(&params, &param_count,
                                &param_capacity, param)) {
                mark_out_of_memory(parser);
                token_array_free(params, param_count);
                return NULL;
            }
        } while (match_one(parser, COMMA));
    }

    if (consume(parser, RIGHT_PAREN,
                "Expect ')' after parameters.") == NULL) {
        token_array_free(params, param_count);
        return NULL;
    }

    if (consume(parser, LEFT_BRACE, "Expect '{' before body.") == NULL) {
        token_array_free(params, param_count);
        return NULL;
    }

    if (!block_contents(parser, &body)) {
        token_array_free(params, param_count);
        stmt_list_free(&body);
        return NULL;
    }

    result = new_function_stmt(*name, params, param_count, body);
    if (result == NULL) {
        mark_out_of_memory(parser);
        token_array_free(params, param_count);
        stmt_list_free(&body);
    }
    return result;
}

static Stmt *var_declaration(Parser *parser)
{
    const Token *name = consume(parser, IDENTIFIER, "Expect variable name.");
    Expr *initializer = NULL;
    Stmt *stmt;

    if (name == NULL) {
        return NULL;
    }

    if (match_one(parser, EQUAL)) {
        initializer = expression(parser);
        if (parser->panic_mode || parser->out_of_memory || initializer == NULL) {
            free_expr(initializer);
            return NULL;
        }
    }

    if (consume(parser, SEMICOLON,
                "Expect ';' after variable declaration.") == NULL) {
        free_expr(initializer);
        return NULL;
    }

    stmt = new_var_stmt(*name, initializer);
    if (stmt == NULL) {
        mark_out_of_memory(parser);
        free_expr(initializer);
    }
    return stmt;
}

/* ================================================================
 * Statements
 * ================================================================ */

static Stmt *statement(Parser *parser)
{
    Stmt *stmt;

    if (match_one(parser, FOR)) {
        return for_statement(parser);
    }
    if (match_one(parser, IF)) {
        return if_statement(parser);
    }
    if (match_one(parser, PRINT)) {
        return print_statement(parser);
    }
    if (match_one(parser, RETURN)) {
        return return_statement(parser);
    }
    if (match_one(parser, WHILE)) {
        return while_statement(parser);
    }
    if (match_one(parser, LEFT_BRACE)) {
        StmtList statements = {0};

        if (!block_contents(parser, &statements)) {
            stmt_list_free(&statements);
            return NULL;
        }

        stmt = new_block_stmt(statements);
        if (stmt == NULL) {
            mark_out_of_memory(parser);
            stmt_list_free(&statements);
        }
        return stmt;
    }

    {
        Expr *expr = expression(parser);
        if (parser->panic_mode || parser->out_of_memory || expr == NULL) {
            free_expr(expr);
            return NULL;
        }

        if (consume(parser, SEMICOLON,
                    "Expect ';' after expression.") == NULL) {
            free_expr(expr);
            return NULL;
        }

        stmt = new_expression_stmt(expr);
        if (stmt == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
        }
        return stmt;
    }
}

static Stmt *for_statement(Parser *parser)
{
    Stmt *initializer = NULL;
    Expr *condition = NULL;
    Expr *increment = NULL;
    Stmt *body = NULL;
    Stmt *result;

    if (consume(parser, LEFT_PAREN, "Expect '(' after 'for'.") == NULL) {
        return NULL;
    }

    if (match_one(parser, SEMICOLON)) {
        initializer = NULL;
    } else if (match_one(parser, VAR)) {
        initializer = var_declaration(parser);
        if (initializer == NULL || parser->panic_mode || parser->out_of_memory) {
            free_stmt(initializer);
            return NULL;
        }
    } else {
        Expr *init_expr = expression(parser);
        if (init_expr == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(init_expr);
            return NULL;
        }
        if (consume(parser, SEMICOLON,
                    "Expect ';' after loop initializer.") == NULL) {
            free_expr(init_expr);
            return NULL;
        }
        initializer = new_expression_stmt(init_expr);
        if (initializer == NULL) {
            mark_out_of_memory(parser);
            free_expr(init_expr);
            return NULL;
        }
    }

    if (!check(parser, SEMICOLON)) {
        condition = expression(parser);
        if (condition == NULL || parser->panic_mode || parser->out_of_memory) {
            free_stmt(initializer);
            free_expr(condition);
            return NULL;
        }
    }

    if (consume(parser, SEMICOLON,
                "Expect ';' after loop condition.") == NULL) {
        free_stmt(initializer);
        free_expr(condition);
        return NULL;
    }

    if (!check(parser, RIGHT_PAREN)) {
        increment = expression(parser);
        if (increment == NULL || parser->panic_mode || parser->out_of_memory) {
            free_stmt(initializer);
            free_expr(condition);
            free_expr(increment);
            return NULL;
        }
    }

    if (consume(parser, RIGHT_PAREN,
                "Expect ')' after clauses.") == NULL) {
        free_stmt(initializer);
        free_expr(condition);
        free_expr(increment);
        return NULL;
    }

    body = statement(parser);
    if (body == NULL || parser->panic_mode || parser->out_of_memory) {
        free_stmt(initializer);
        free_expr(condition);
        free_expr(increment);
        free_stmt(body);
        return NULL;
    }

    if (increment != NULL) {
        StmtList statements = {0};
        Stmt *increment_stmt = new_expression_stmt(increment);

        if (increment_stmt == NULL) {
            mark_out_of_memory(parser);
            free_stmt(initializer);
            free_expr(condition);
            free_expr(increment);
            free_stmt(body);
            return NULL;
        }

        if (!stmt_list_push(&statements, body)) {
            mark_out_of_memory(parser);
            free_stmt(body);
            free_stmt(increment_stmt);
            stmt_list_free(&statements);
            free_stmt(initializer);
            free_expr(condition);
            return NULL;
        }
        body = NULL; /* ownership moved to the list */

        if (!stmt_list_push(&statements, increment_stmt)) {
            mark_out_of_memory(parser);
            free_stmt(increment_stmt);
            stmt_list_free(&statements);
            free_stmt(initializer);
            free_expr(condition);
            return NULL;
        }

        body = new_block_stmt(statements);
        if (body == NULL) {
            mark_out_of_memory(parser);
            stmt_list_free(&statements);
            free_stmt(initializer);
            free_expr(condition);
            return NULL;
        }
    }

    if (condition == NULL) {
        condition = new_literal_bool(true);
        if (condition == NULL) {
            mark_out_of_memory(parser);
            free_stmt(initializer);
            free_stmt(body);
            return NULL;
        }
    }

    result = new_while_stmt(condition, body);
    if (result == NULL) {
        mark_out_of_memory(parser);
        free_expr(condition);
        free_stmt(body);
        free_stmt(initializer);
        return NULL;
    }
    condition = NULL;
    body = NULL;

    if (initializer != NULL) {
        StmtList statements = {0};
        if (!stmt_list_push(&statements, initializer)) {
            mark_out_of_memory(parser);
            free_stmt(initializer);
            free_stmt(result);
            return NULL;
        }
        initializer = NULL;

        if (!stmt_list_push(&statements, result)) {
            mark_out_of_memory(parser);
            free_stmt(result);
            stmt_list_free(&statements);
            return NULL;
        }
        result = new_block_stmt(statements);
        if (result == NULL) {
            mark_out_of_memory(parser);
            stmt_list_free(&statements);
            return NULL;
        }
    }

    return result;
}

static Stmt *if_statement(Parser *parser)
{
    Expr *condition;
    Stmt *then_branch;
    Stmt *else_branch = NULL;
    Stmt *stmt;

    if (consume(parser, LEFT_PAREN, "Expect '(' after 'if'.") == NULL) {
        return NULL;
    }

    condition = expression(parser);
    if (condition == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(condition);
        return NULL;
    }

    if (consume(parser, RIGHT_PAREN,
                "Expect ')' after condition.") == NULL) {
        free_expr(condition);
        return NULL;
    }

    then_branch = statement(parser);
    if (then_branch == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(condition);
        free_stmt(then_branch);
        return NULL;
    }

    if (match_one(parser, ELSE)) {
        else_branch = statement(parser);
        if (else_branch == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(condition);
            free_stmt(then_branch);
            free_stmt(else_branch);
            return NULL;
        }
    }

    stmt = new_if_stmt(condition, then_branch, else_branch);
    if (stmt == NULL) {
        mark_out_of_memory(parser);
        free_expr(condition);
        free_stmt(then_branch);
        free_stmt(else_branch);
    }
    return stmt;
}

static Stmt *print_statement(Parser *parser)
{
    Expr *expr = expression(parser);
    Stmt *stmt;

    if (expr == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(expr);
        return NULL;
    }

    if (consume(parser, SEMICOLON, "Expect ';' after value.") == NULL) {
        free_expr(expr);
        return NULL;
    }

    stmt = new_print_stmt(expr);
    if (stmt == NULL) {
        mark_out_of_memory(parser);
        free_expr(expr);
    }
    return stmt;
}

static Stmt *return_statement(Parser *parser)
{
    const Token *keyword = previous(parser);
    Expr *value = NULL;
    Stmt *stmt;

    if (!check(parser, SEMICOLON)) {
        value = expression(parser);
        if (value == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(value);
            return NULL;
        }
    }

    if (consume(parser, SEMICOLON,
                "Expect ';' after return value.") == NULL) {
        free_expr(value);
        return NULL;
    }

    stmt = new_return_stmt(*keyword, value);
    if (stmt == NULL) {
        mark_out_of_memory(parser);
        free_expr(value);
    }
    return stmt;
}

static Stmt *while_statement(Parser *parser)
{
    Expr *condition;
    Stmt *body;
    Stmt *stmt;

    if (consume(parser, LEFT_PAREN, "Expect '(' after 'while'.") == NULL) {
        return NULL;
    }

    condition = expression(parser);
    if (condition == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(condition);
        return NULL;
    }

    if (consume(parser, RIGHT_PAREN,
                "Expect ')' after condition.") == NULL) {
        free_expr(condition);
        return NULL;
    }

    body = statement(parser);
    if (body == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(condition);
        free_stmt(body);
        return NULL;
    }

    stmt = new_while_stmt(condition, body);
    if (stmt == NULL) {
        mark_out_of_memory(parser);
        free_expr(condition);
        free_stmt(body);
    }
    return stmt;
}

static bool block_contents(Parser *parser, StmtList *statements)
{
    if (parser == NULL || statements == NULL) {
        return false;
    }

    while (!check(parser, RIGHT_BRACE) && !is_at_end(parser)) {
        Stmt *stmt = declaration(parser);

        if (parser->out_of_memory) {
            free_stmt(stmt);
            return false;
        }
        if (stmt != NULL && !stmt_list_push(statements, stmt)) {
            free_stmt(stmt);
            mark_out_of_memory(parser);
            return false;
        }
    }

    if (consume(parser, RIGHT_BRACE, "Expect '}' after block.") == NULL) {
        return false;
    }
    return true;
}

/* ================================================================
 * Expressions: precedence from low to high
 * assignment -> or -> and -> equality -> comparison -> term -> factor
 * -> unary -> call/property -> primary
 * ================================================================ */

static Expr *expression(Parser *parser)
{
    return assignment(parser);
}

static Expr *assignment(Parser *parser)
{
    Expr *expr = logic_or(parser);
    Expr *value;
    Expr *result = NULL;

    if (expr == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(expr);
        return NULL;
    }

    if (!match_one(parser, EQUAL)) {
        return expr;
    }

    value = assignment(parser);
    if (value == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(expr);
        free_expr(value);
        return NULL;
    }

    if (expr->type == EXPR_VARIABLE) {
        result = new_assign_expr(expr->as.variable.name, value);
        if (result == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(value);
            return NULL;
        }
        free_expr(expr);
        return result;
    }

    if (expr->type == EXPR_GET) {
        result = new_set_expr(expr->as.get.object, expr->as.get.name, value);
        if (result == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(value);
            return NULL;
        }

        /* new_set_expr adopted the object pointer; don't free it twice. */
        expr->as.get.object = NULL;
        free_expr(expr);
        return result;
    }

    error_at(parser, previous(parser), "Invalid assignment target.");
    free_expr(expr);
    free_expr(value);
    return NULL;
}

static Expr *logic_or(Parser *parser)
{
    static const TokenType types[] = {OR};
    Expr *expr = logic_and(parser);

    while (expr != NULL && !parser->panic_mode && !parser->out_of_memory &&
           match_any(parser, types, sizeof(types) / sizeof(types[0]))) {
        const Token *op = previous(parser);
        Expr *right = logic_and(parser);
        Expr *combined;

        if (right == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        combined = new_logical_expr(expr, *op, right);
        if (combined == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        expr = combined;
    }

    return expr;
}

static Expr *logic_and(Parser *parser)
{
    static const TokenType types[] = {AND};
    Expr *expr = equality(parser);

    while (expr != NULL && !parser->panic_mode && !parser->out_of_memory &&
           match_any(parser, types, sizeof(types) / sizeof(types[0]))) {
        const Token *op = previous(parser);
        Expr *right = equality(parser);
        Expr *combined;

        if (right == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(expr);
            free_expr(right);
            return NULL;
        }

        combined = new_logical_expr(expr, *op, right);
        if (combined == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        expr = combined;
    }

    return expr;
}

static Expr *equality(Parser *parser)
{
    static const TokenType types[] = {BANG_EQUAL, EQUAL_EQUAL};
    Expr *expr = comparison(parser);

    while (expr != NULL && !parser->panic_mode && !parser->out_of_memory &&
           match_any(parser, types, sizeof(types) / sizeof(types[0]))) {
        const Token *op = previous(parser);
        Expr *right = comparison(parser);
        Expr *combined;

        if (right == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        combined = new_binary_expr(expr, *op, right);
        if (combined == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        expr = combined;
    }
    return expr;
}

static Expr *comparison(Parser *parser)
{
    static const TokenType types[] = {
        GREATER, GREATER_EQUAL, LESS, LESS_EQUAL
    };
    Expr *expr = term(parser);

    while (expr != NULL && !parser->panic_mode && !parser->out_of_memory &&
           match_any(parser, types, sizeof(types) / sizeof(types[0]))) {
        const Token *op = previous(parser);
        Expr *right = term(parser);
        Expr *combined;

        if (right == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        combined = new_binary_expr(expr, *op, right);
        if (combined == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        expr = combined;
    }
    return expr;
}

static Expr *term(Parser *parser)
{
    static const TokenType types[] = {MINUS, PLUS};
    Expr *expr = factor(parser);

    while (expr != NULL && !parser->panic_mode && !parser->out_of_memory &&
           match_any(parser, types, sizeof(types) / sizeof(types[0]))) {
        const Token *op = previous(parser);
        Expr *right = factor(parser);
        Expr *combined;

        if (right == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        combined = new_binary_expr(expr, *op, right);
        if (combined == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        expr = combined;
    }
    return expr;
}

static Expr *factor(Parser *parser)
{
    static const TokenType types[] = {SLASH, STAR};
    Expr *expr = unary(parser);

    while (expr != NULL && !parser->panic_mode && !parser->out_of_memory &&
           match_any(parser, types, sizeof(types) / sizeof(types[0]))) {
        const Token *op = previous(parser);
        Expr *right = unary(parser);
        Expr *combined;

        if (right == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        combined = new_binary_expr(expr, *op, right);
        if (combined == NULL) {
            mark_out_of_memory(parser);
            free_expr(expr);
            free_expr(right);
            return NULL;
        }
        expr = combined;
    }
    return expr;
}

static Expr *unary(Parser *parser)
{
    static const TokenType types[] = {BANG, MINUS};

    if (match_any(parser, types, sizeof(types) / sizeof(types[0]))) {
        const Token *op = previous(parser);
        Expr *right = unary(parser);
        Expr *expr;

        if (right == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(right);
            return NULL;
        }

        expr = new_unary_expr(*op, right);
        if (expr == NULL) {
            mark_out_of_memory(parser);
            free_expr(right);
        }
        return expr;
    }

    return call(parser);
}

static Expr *call(Parser *parser)
{
    Expr *expr = primary(parser);

    if (expr == NULL || parser->panic_mode || parser->out_of_memory) {
        free_expr(expr);
        return NULL;
    }

    for (;;) {
        if (match_one(parser, LEFT_PAREN)) {
            Expr *call_expr = finish_call(parser, expr);
            if (call_expr == NULL) {
                return NULL;
            }
            expr = call_expr;
        } else if (match_one(parser, DOT)) {
            const Token *name = consume(
                parser, IDENTIFIER, "Expect property name.");
            Expr *get_expr;

            if (name == NULL) {
                free_expr(expr);
                return NULL;
            }

            get_expr = new_get_expr(expr, *name);
            if (get_expr == NULL) {
                mark_out_of_memory(parser);
                free_expr(expr);
                return NULL;
            }
            expr = get_expr;
        } else {
            break;
        }
    }

    return expr;
}

static Expr *finish_call(Parser *parser, Expr *callee)
{
    ExprList arguments = {0};
    const Token *paren;
    Expr *result;

    if (!check(parser, RIGHT_PAREN)) {
        do {
            Expr *argument = expression(parser);

            if (argument == NULL || parser->panic_mode || parser->out_of_memory) {
                free_expr(argument);
                free_expr(callee);
                expr_list_free(&arguments);
                return NULL;
            }

            if (!expr_list_push(&arguments, argument)) {
                mark_out_of_memory(parser);
                free_expr(argument);
                free_expr(callee);
                expr_list_free(&arguments);
                return NULL;
            }
        } while (match_one(parser, COMMA));
    }

    paren = consume(parser, RIGHT_PAREN, "Expect ')' after arguments.");
    if (paren == NULL) {
        free_expr(callee);
        expr_list_free(&arguments);
        return NULL;
    }

    result = new_call_expr(callee, *paren, arguments);
    if (result == NULL) {
        mark_out_of_memory(parser);
        free_expr(callee);
        expr_list_free(&arguments);
    }
    return result;
}

static Expr *primary(Parser *parser)
{
    const Token *token;
    Expr *expr;

    if (match_one(parser, FALSE)) {
        expr = new_literal_bool(false);
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, TRUE)) {
        expr = new_literal_bool(true);
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, NIL)) {
        expr = new_literal_nil();
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, NUMBER)) {
        token = previous(parser);
        expr = new_literal_number(strtod(
            token->lexeme != NULL ? token->lexeme : "", NULL));
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, STRING)) {
        const char *value;
        size_t length;

        token = previous(parser);
        value = token->lexeme != NULL ? token->lexeme : "";
        length = strlen(value);

        /* Match the C++ parser's optional surrounding-quote removal. */
        if (length >= 2 && value[0] == '\"' && value[length - 1] == '\"') {
            ++value;
            length -= 2;
        }

        expr = new_literal_string(value, length);
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, THIS)) {
        expr = new_this_expr(*previous(parser));
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, SUPER)) {
        const Token *keyword = previous(parser);
        const Token *method;

        if (consume(parser, DOT, "Expect '.' after 'super'.") == NULL) {
            return NULL;
        }

        method = consume(parser, IDENTIFIER,
                         "Expect superclass method name.");
        if (method == NULL) {
            return NULL;
        }

        expr = new_super_expr(*keyword, *method);
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, IDENTIFIER)) {
        expr = new_variable_expr(*previous(parser));
        if (expr == NULL) mark_out_of_memory(parser);
        return expr;
    }

    if (match_one(parser, LEFT_PAREN)) {
        Expr *inside = expression(parser);

        if (inside == NULL || parser->panic_mode || parser->out_of_memory) {
            free_expr(inside);
            return NULL;
        }

        if (consume(parser, RIGHT_PAREN,
                    "Expect ')' after expression.") == NULL) {
            free_expr(inside);
            return NULL;
        }

        expr = new_grouping_expr(inside);
        if (expr == NULL) {
            mark_out_of_memory(parser);
            free_expr(inside);
        }
        return expr;
    }

    error_at(parser, peek(parser), "Expect expression.");
    return NULL;
}

/* ================================================================
 * Public API
 * ================================================================ */

void parser_init(Parser *parser, const Token *tokens, size_t token_count)
{
    if (parser == NULL) {
        return;
    }

    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current = 0;
    parser->panic_mode = false;
    parser->had_error = false;
    parser->out_of_memory = false;
}

bool parser_parse(Parser *parser, StmtList *out_statements)
{
    if (parser == NULL || out_statements == NULL ||
        (parser->tokens == NULL && parser->token_count != 0) ||
        out_statements->items != NULL || out_statements->count != 0 ||
        out_statements->capacity != 0) {
        return false;
    }

    while (!is_at_end(parser)) {
        size_t before = parser->current;
        Stmt *stmt = declaration(parser);

        if (parser->out_of_memory) {
            free_stmt(stmt);
            stmt_list_free(out_statements);
            return false;
        }

        if (stmt != NULL && !stmt_list_push(out_statements, stmt)) {
            free_stmt(stmt);
            mark_out_of_memory(parser);
            stmt_list_free(out_statements);
            return false;
        }

        /* Defensive progress guard for malformed/non-EOF-terminated input. */
        if (parser->current == before && !is_at_end(parser)) {
            (void)advance(parser);
        }
    }

    return !parser->had_error && !parser->out_of_memory;
}
