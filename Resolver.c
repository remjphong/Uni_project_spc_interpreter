#include "resolver.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *resolver_copy_string(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        text = "";
    }

    length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }

    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static FILE *resolver_error_stream(const Resolver *resolver)
{
    if (resolver != NULL && resolver->interpreter != NULL &&
        resolver->interpreter->err != NULL) {
        return resolver->interpreter->err;
    }
    return stderr;
}

static bool grow_array(void **array, size_t *capacity,
                       size_t needed, size_t element_size)
{
    size_t new_capacity;
    void *new_array;

    if (needed <= *capacity) {
        return true;
    }
    if (element_size == 0 || needed > SIZE_MAX / element_size) {
        return false;
    }

    new_capacity = *capacity;
    if (new_capacity == 0) {
        new_capacity = 4;
    }
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2;
    }
    if (new_capacity > SIZE_MAX / element_size) {
        return false;
    }

    new_array = realloc(*array, new_capacity * element_size);
    if (new_array == NULL) {
        return false;
    }

    *array = new_array;
    *capacity = new_capacity;
    return true;
}

static void resolver_memory_error(Resolver *resolver)
{
    if (resolver == NULL) {
        return;
    }
    resolver->out_of_memory = true;
    resolver->had_error = true;
    fputs("Resolver error: not enough memory.\n", resolver_error_stream(resolver));
}

void resolver_error(Resolver *resolver, Token token, const char *message)
{
    FILE *err;
    const char *lexeme;

    if (resolver == NULL) {
        return;
    }

    resolver->had_error = true;
    err = resolver_error_stream(resolver);
    lexeme = token.lexeme != NULL ? token.lexeme : "";
    fprintf(err, "[line %d] Error at '%s': %s\n",
            token.line, lexeme,
            message != NULL ? message : "Unknown resolver error.");
}

void resolver_init(Resolver *resolver, Interpreter *interpreter)
{
    if (resolver == NULL) {
        return;
    }

    *resolver = (Resolver){0};
    resolver->interpreter = interpreter;
    resolver->current_function = RESOLVER_FUNCTION_NONE;
    resolver->current_class = RESOLVER_CLASS_NONE;
}

bool resolver_begin_scope(Resolver *resolver)
{
    if (resolver == NULL || resolver->out_of_memory) {
        return false;
    }
    if (resolver->scope_count == SIZE_MAX ||
        !grow_array((void **)&resolver->scopes,
                    &resolver->scope_capacity,
                    resolver->scope_count + 1,
                    sizeof(*resolver->scopes))) {
        resolver_memory_error(resolver);
        return false;
    }

    resolver->scopes[resolver->scope_count] = (Scope){0};
    resolver->scope_count++;
    return true;
}

void resolver_end_scope(Resolver *resolver)
{
    Scope *scope;
    size_t i;

    if (resolver == NULL || resolver->scope_count == 0) {
        return;
    }

    scope = &resolver->scopes[resolver->scope_count - 1];
    for (i = 0; i < scope->count; ++i) {
        free(scope->entries[i].name);
    }
    free(scope->entries);
    *scope = (Scope){0};
    resolver->scope_count--;
}

bool resolver_declare(Resolver *resolver, Token name)
{
    Scope *scope;
    const char *name_text;
    size_t i;
    char *name_copy;

    if (resolver == NULL || resolver->out_of_memory) {
        return false;
    }

    /* Top-level variables are globals and are handled by the runtime. */
    if (resolver->scope_count == 0) {
        return true;
    }

    scope = &resolver->scopes[resolver->scope_count - 1];
    name_text = name.lexeme != NULL ? name.lexeme : "";

    for (i = 0; i < scope->count; ++i) {
        if (strcmp(scope->entries[i].name, name_text) == 0) {
            resolver_error(resolver, name,
                           "Already a variable with this name in this scope.");
            /* Do not add a duplicate entry. The caller may still resolve the
             * initializer to report additional errors, as the C++ version does. */
            return true;
        }
    }

    if (scope->count == SIZE_MAX ||
        !grow_array((void **)&scope->entries, &scope->capacity,
                    scope->count + 1, sizeof(*scope->entries))) {
        resolver_memory_error(resolver);
        return false;
    }

    name_copy = resolver_copy_string(name_text);
    if (name_copy == NULL) {
        resolver_memory_error(resolver);
        return false;
    }

    scope->entries[scope->count].name = name_copy;
    scope->entries[scope->count].defined = false;
    scope->count++;
    return true;
}

bool resolver_define(Resolver *resolver, Token name)
{
    Scope *scope;
    const char *name_text;
    size_t i;
    char *name_copy;

    if (resolver == NULL || resolver->out_of_memory) {
        return false;
    }
    if (resolver->scope_count == 0) {
        return true;
    }

    scope = &resolver->scopes[resolver->scope_count - 1];
    name_text = name.lexeme != NULL ? name.lexeme : "";

    for (i = 0; i < scope->count; ++i) {
        if (strcmp(scope->entries[i].name, name_text) == 0) {
            scope->entries[i].defined = true;
            return true;
        }
    }

    /* Defensive fallback if define() is called without declare(). */
    if (scope->count == SIZE_MAX ||
        !grow_array((void **)&scope->entries, &scope->capacity,
                    scope->count + 1, sizeof(*scope->entries))) {
        resolver_memory_error(resolver);
        return false;
    }
    name_copy = resolver_copy_string(name_text);
    if (name_copy == NULL) {
        resolver_memory_error(resolver);
        return false;
    }

    scope->entries[scope->count].name = name_copy;
    scope->entries[scope->count].defined = true;
    scope->count++;
    return true;
}

static bool scope_has_undefined_name(const Scope *scope, const char *name)
{
    size_t i;

    if (scope == NULL || name == NULL) {
        return false;
    }
    for (i = 0; i < scope->count; ++i) {
        if (strcmp(scope->entries[i].name, name) == 0) {
            return !scope->entries[i].defined;
        }
    }
    return false;
}

static bool record_binding(Resolver *resolver, const Expr *expression,
                           size_t distance)
{
    size_t i;

    for (i = 0; i < resolver->binding_count; ++i) {
        if (resolver->bindings[i].expression == expression) {
            resolver->bindings[i].distance = distance;
            return true;
        }
    }

    if (resolver->binding_count == SIZE_MAX ||
        !grow_array((void **)&resolver->bindings,
                    &resolver->binding_capacity,
                    resolver->binding_count + 1,
                    sizeof(*resolver->bindings))) {
        resolver_memory_error(resolver);
        return false;
    }

    resolver->bindings[resolver->binding_count].expression = expression;
    resolver->bindings[resolver->binding_count].distance = distance;
    resolver->binding_count++;
    return true;
}

void resolver_resolve_local(Resolver *resolver, Expr *expr, Token name)
{
    const char *name_text;
    size_t scope_index;

    if (resolver == NULL || expr == NULL || resolver->out_of_memory) {
        return;
    }

    name_text = name.lexeme != NULL ? name.lexeme : "";
    scope_index = resolver->scope_count;

    /* Search the innermost scope first, matching the C++ Resolver. */
    while (scope_index > 0) {
        size_t current_index = scope_index - 1;
        const Scope *scope = &resolver->scopes[current_index];
        size_t entry_index;

        for (entry_index = 0; entry_index < scope->count; ++entry_index) {
            if (strcmp(scope->entries[entry_index].name, name_text) == 0) {
                size_t distance = resolver->scope_count - 1 - current_index;
                (void)record_binding(resolver, expr, distance);
                return;
            }
        }
        scope_index--;
    }

    /* No local binding found: treat it as a global variable. */
}

bool resolver_get_distance(const Resolver *resolver,
                           const Expr *expression,
                           size_t *distance_out)
{
    size_t i;

    if (resolver == NULL || expression == NULL || distance_out == NULL) {
        return false;
    }
    for (i = 0; i < resolver->binding_count; ++i) {
        if (resolver->bindings[i].expression == expression) {
            *distance_out = resolver->bindings[i].distance;
            return true;
        }
    }
    return false;
}

void resolver_resolve_expr(Resolver *resolver, Expr *expr)
{
    if (resolver == NULL || expr == NULL || resolver->out_of_memory) {
        return;
    }

    switch (expr->type) {
        case EXPR_BINARY:
            resolver_resolve_expr(resolver, expr->as.binary.left);
            resolver_resolve_expr(resolver, expr->as.binary.right);
            break;

        case EXPR_UNARY:
            resolver_resolve_expr(resolver, expr->as.unary.right);
            break;

        case EXPR_LITERAL:
            /* Literal values contain no identifiers. */
            break;

        case EXPR_GROUPING:
            resolver_resolve_expr(resolver, expr->as.grouping.expression);
            break;

        case EXPR_VARIABLE: {
            Token name = expr->as.variable.name;
            const char *name_text = name.lexeme != NULL ? name.lexeme : "";

            if (resolver->scope_count > 0 &&
                scope_has_undefined_name(
                    &resolver->scopes[resolver->scope_count - 1], name_text)) {
                resolver_error(resolver, name,
                               "Can't read local variable in its own initializer.");
            }
            resolver_resolve_local(resolver, expr, name);
            break;
        }

        default:
            resolver_error(resolver, (Token){0},
                           "Unsupported expression node in current AST.");
            break;
    }
}

void resolver_resolve_stmt(Resolver *resolver, Stmt *stmt)
{
    if (resolver == NULL || stmt == NULL || resolver->out_of_memory) {
        return;
    }

    switch (stmt->type) {
        case STMT_EXPRESSION:
            resolver_resolve_expr(resolver, stmt->as.expression.expression);
            break;

        case STMT_PRINT:
            resolver_resolve_expr(resolver, stmt->as.print.expression);
            break;

        case STMT_VAR: {
            Token name = stmt->as.var.name;

            if (!resolver_declare(resolver, name)) {
                return;
            }
            if (stmt->as.var.initializer != NULL) {
                resolver_resolve_expr(resolver, stmt->as.var.initializer);
            }
            (void)resolver_define(resolver, name);
            break;
        }

        default:
            resolver_error(resolver, (Token){0},
                           "Unsupported statement node in current AST.");
            break;
    }
}

void resolver_resolve_statements(Resolver *resolver,
                                 Stmt *const *statements,
                                 size_t statement_count)
{
    size_t i;

    if (resolver == NULL || resolver->out_of_memory ||
        (statement_count > 0 && statements == NULL)) {
        return;
    }

    for (i = 0; i < statement_count; ++i) {
        if (resolver->out_of_memory) {
            return;
        }
        resolver_resolve_stmt(resolver, statements[i]);
    }
}

void resolver_free(Resolver *resolver)
{
    if (resolver == NULL) {
        return;
    }
    while (resolver->scope_count > 0) {
        resolver_end_scope(resolver);
    }
    free(resolver->scopes);
    free(resolver->bindings);
    *resolver = (Resolver){0};
}
