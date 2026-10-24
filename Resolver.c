#include "resolver.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------
 * Utilities and errors
 * --------------------------------------------------------- */

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
    if (resolver != NULL && resolver->error_stream != NULL) {
        return resolver->error_stream;
    }
    return stderr;
}

static bool grow_array(
    void **array,
    size_t *capacity,
    size_t needed,
    size_t element_size
)
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

static Token synthetic_token(TokenType type, const char *lexeme)
{
    Token token;
    token.type = type;
    token.lexeme = (char *)(lexeme != NULL ? lexeme : "");
    token.line = 0;
    token.column = 0;
    return token;
}

static void resolver_memory_error(Resolver *resolver)
{
    if (resolver == NULL || resolver->out_of_memory) {
        return;
    }

    resolver->out_of_memory = true;
    resolver->had_error = true;
    fputs("Resolver error: not enough memory.\n", resolver_error_stream(resolver));
}

void resolver_error(Resolver *resolver, Token token, const char *message)
{
    const char *lexeme;

    if (resolver == NULL) {
        return;
    }

    resolver->had_error = true;
    lexeme = token.lexeme != NULL ? token.lexeme : "";

    fprintf(
        resolver_error_stream(resolver),
        "[line %d] Error at '%s': %s\n",
        token.line,
        lexeme,
        message != NULL ? message : "Unknown resolver error."
    );
}

void resolver_set_error_stream(Resolver *resolver, FILE *stream)
{
    if (resolver != NULL) {
        resolver->error_stream = stream != NULL ? stream : stderr;
    }
}

void resolver_set_binding_sink(
    Resolver *resolver,
    ResolverBindingSink sink,
    void *context
)
{
    if (resolver == NULL) {
        return;
    }

    resolver->binding_sink = sink;
    resolver->binding_sink_context = context != NULL
        ? context
        : (void *)resolver->interpreter;

    /* Synchronize bindings discovered before a sink was installed. */
    if (sink != NULL) {
        size_t i;
        for (i = 0; i < resolver->binding_count; ++i) {
            sink(
                resolver->binding_sink_context,
                resolver->bindings[i].expression,
                resolver->bindings[i].distance
            );
        }
    }
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
    resolver->error_stream = stderr;
    resolver->binding_sink_context = (void *)interpreter;
}

bool resolver_had_error(const Resolver *resolver)
{
    return resolver != NULL && resolver->had_error;
}

bool resolver_out_of_memory(const Resolver *resolver)
{
    return resolver != NULL && resolver->out_of_memory;
}

/* ---------------------------------------------------------
 * Scope operations
 * --------------------------------------------------------- */

bool resolver_begin_scope(Resolver *resolver)
{
    if (resolver == NULL || resolver->out_of_memory) {
        return false;
    }

    if (resolver->scope_count == SIZE_MAX ||
        !grow_array(
            (void **)&resolver->scopes,
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

    /* Globals are maintained by the global environment, not local scopes. */
    if (resolver->scope_count == 0) {
        return true;
    }

    scope = &resolver->scopes[resolver->scope_count - 1];
    name_text = name.lexeme != NULL ? name.lexeme : "";

    for (i = 0; i < scope->count; ++i) {
        if (strcmp(scope->entries[i].name, name_text) == 0) {
            resolver_error(
                resolver,
                name,
                "Already a variable with this name in this scope."
            );
            /* Keep the first declaration's state; do not overwrite it. */
            return true;
        }
    }

    if (scope->count == SIZE_MAX ||
        !grow_array(
            (void **)&scope->entries,
            &scope->capacity,
            scope->count + 1,
            sizeof(*scope->entries))) {
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

    /* Defensive fallback when define() is used without a prior declare(). */
    if (scope->count == SIZE_MAX ||
        !grow_array(
            (void **)&scope->entries,
            &scope->capacity,
            scope->count + 1,
            sizeof(*scope->entries))) {
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

static bool scope_define_synthetic(Resolver *resolver, const char *name)
{
    Token token = synthetic_token(IDENTIFIER, name);
    return resolver_declare(resolver, token) && resolver_define(resolver, token);
}

/* ---------------------------------------------------------
 * Local binding table
 * --------------------------------------------------------- */

static bool record_binding(
    Resolver *resolver,
    const Expr *expression,
    size_t distance
)
{
    size_t i;

    for (i = 0; i < resolver->binding_count; ++i) {
        if (resolver->bindings[i].expression == expression) {
            resolver->bindings[i].distance = distance;
            if (resolver->binding_sink != NULL) {
                resolver->binding_sink(
                    resolver->binding_sink_context,
                    expression,
                    distance
                );
            }
            return true;
        }
    }

    if (resolver->binding_count == SIZE_MAX ||
        !grow_array(
            (void **)&resolver->bindings,
            &resolver->binding_capacity,
            resolver->binding_count + 1,
            sizeof(*resolver->bindings))) {
        resolver_memory_error(resolver);
        return false;
    }

    resolver->bindings[resolver->binding_count].expression = expression;
    resolver->bindings[resolver->binding_count].distance = distance;
    resolver->binding_count++;

    if (resolver->binding_sink != NULL) {
        resolver->binding_sink(
            resolver->binding_sink_context,
            expression,
            distance
        );
    }
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

    /* Not found in a local scope: this name is global. */
}

bool resolver_get_distance(
    const Resolver *resolver,
    const Expr *expression,
    size_t *distance_out
)
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

/* ---------------------------------------------------------
 * Function resolution
 * --------------------------------------------------------- */

static void resolver_resolve_function(
    Resolver *resolver,
    const Stmt *function,
    ResolverFunctionType type
)
{
    ResolverFunctionType enclosing_function;
    bool scope_started;
    size_t i;

    if (resolver == NULL || function == NULL ||
        function->type != STMT_FUNCTION || resolver->out_of_memory) {
        return;
    }

    enclosing_function = resolver->current_function;
    resolver->current_function = type;

    scope_started = resolver_begin_scope(resolver);
    if (scope_started) {
        for (i = 0; i < function->as.function.param_count; ++i) {
            Token parameter = function->as.function.params[i];
            if (!resolver_declare(resolver, parameter)) {
                break;
            }
            if (!resolver_define(resolver, parameter)) {
                break;
            }
        }

        resolver_resolve_statements(
            resolver,
            function->as.function.body.items,
            function->as.function.body.count
        );
        resolver_end_scope(resolver);
    }

    /* Restore context even if a memory error occurred. */
    resolver->current_function = enclosing_function;
}

/* ---------------------------------------------------------
 * Expression resolver
 * --------------------------------------------------------- */

void resolver_resolve_expr(Resolver *resolver, Expr *expr)
{
    if (resolver == NULL || expr == NULL || resolver->out_of_memory) {
        return;
    }

    switch (expr->type) {
        case EXPR_ASSIGN:
            /* Match C++: resolve RHS before recording assignment target. */
            resolver_resolve_expr(resolver, expr->as.assign.value);
            resolver_resolve_local(resolver, expr, expr->as.assign.name);
            break;

        case EXPR_BINARY:
            resolver_resolve_expr(resolver, expr->as.binary.left);
            resolver_resolve_expr(resolver, expr->as.binary.right);
            break;

        case EXPR_CALL: {
            size_t i;
            resolver_resolve_expr(resolver, expr->as.call.callee);
            for (i = 0; i < expr->as.call.arguments.count; ++i) {
                resolver_resolve_expr(resolver, expr->as.call.arguments.items[i]);
                if (resolver->out_of_memory) {
                    return;
                }
            }
            break;
        }

        case EXPR_GET:
            resolver_resolve_expr(resolver, expr->as.get.object);
            break;

        case EXPR_GROUPING:
            resolver_resolve_expr(resolver, expr->as.grouping.expression);
            break;

        case EXPR_LITERAL:
            /* Literals contain no lexical references. */
            break;

        case EXPR_LOGICAL:
            resolver_resolve_expr(resolver, expr->as.logical.left);
            resolver_resolve_expr(resolver, expr->as.logical.right);
            break;

        case EXPR_SET:
            /* Match C++ visitor order: value first, then object. */
            resolver_resolve_expr(resolver, expr->as.set.value);
            resolver_resolve_expr(resolver, expr->as.set.object);
            break;

        case EXPR_SUPER:
            if (resolver->current_class == RESOLVER_CLASS_NONE) {
                resolver_error(
                    resolver,
                    expr->as.super_expr.keyword,
                    "Can't use 'super' outside of a class."
                );
            } else if (resolver->current_class != RESOLVER_CLASS_SUBCLASS) {
                resolver_error(
                    resolver,
                    expr->as.super_expr.keyword,
                    "Can't use 'super' in a class with no superclass."
                );
            } else {
                resolver_resolve_local(
                    resolver,
                    expr,
                    expr->as.super_expr.keyword
                );
            }
            break;

        case EXPR_THIS:
            if (resolver->current_class == RESOLVER_CLASS_NONE) {
                resolver_error(
                    resolver,
                    expr->as.this_expr.keyword,
                    "Can't use 'this' outside of a class."
                );
            } else {
                resolver_resolve_local(
                    resolver,
                    expr,
                    expr->as.this_expr.keyword
                );
            }
            break;

        case EXPR_UNARY:
            resolver_resolve_expr(resolver, expr->as.unary.right);
            break;

        case EXPR_VARIABLE: {
            Token name = expr->as.variable.name;
            const char *name_text = name.lexeme != NULL ? name.lexeme : "";

            if (resolver->scope_count > 0 &&
                scope_has_undefined_name(
                    &resolver->scopes[resolver->scope_count - 1],
                    name_text)) {
                resolver_error(
                    resolver,
                    name,
                    "Can't read local variable in its own initializer."
                );
            }

            resolver_resolve_local(resolver, expr, name);
            break;
        }

        default:
            resolver_error(
                resolver,
                synthetic_token(TOKEN_ERROR, ""),
                "Unsupported expression node in current AST."
            );
            break;
    }
}

/* ---------------------------------------------------------
 * Statement resolver
 * --------------------------------------------------------- */

void resolver_resolve_stmt(Resolver *resolver, Stmt *stmt)
{
    if (resolver == NULL || stmt == NULL || resolver->out_of_memory) {
        return;
    }

    switch (stmt->type) {
        case STMT_BLOCK:
            if (resolver_begin_scope(resolver)) {
                resolver_resolve_statements(
                    resolver,
                    stmt->as.block.statements.items,
                    stmt->as.block.statements.count
                );
                resolver_end_scope(resolver);
            }
            break;

        case STMT_CLASS: {
            ResolverClassType enclosing_class = resolver->current_class;
            bool super_scope_started = false;
            bool this_scope_started = false;
            Expr *superclass = stmt->as.class_stmt.superclass;
            size_t i;

            resolver->current_class = RESOLVER_CLASS_CLASS;
            (void)resolver_declare(resolver, stmt->as.class_stmt.name);
            (void)resolver_define(resolver, stmt->as.class_stmt.name);

            if (superclass != NULL) {
                if (superclass->type != EXPR_VARIABLE) {
                    resolver_error(
                        resolver,
                        stmt->as.class_stmt.name,
                        "Internal AST error: superclass must be a variable expression."
                    );
                } else if (strcmp(
                    stmt->as.class_stmt.name.lexeme != NULL
                        ? stmt->as.class_stmt.name.lexeme : "",
                    superclass->as.variable.name.lexeme != NULL
                        ? superclass->as.variable.name.lexeme : "") == 0) {
                    resolver_error(
                        resolver,
                        superclass->as.variable.name,
                        "A class can't inherit from itself."
                    );
                }

                resolver->current_class = RESOLVER_CLASS_SUBCLASS;
                resolver_resolve_expr(resolver, superclass);

                if (resolver_begin_scope(resolver)) {
                    super_scope_started = true;
                    (void)scope_define_synthetic(resolver, "super");
                }
            }

            if (resolver_begin_scope(resolver)) {
                this_scope_started = true;
                (void)scope_define_synthetic(resolver, "this");
            }

            for (i = 0; i < stmt->as.class_stmt.methods.count; ++i) {
                Stmt *method = stmt->as.class_stmt.methods.items[i];
                ResolverFunctionType method_type = RESOLVER_FUNCTION_METHOD;

                if (resolver->out_of_memory) {
                    break;
                }
                if (method == NULL || method->type != STMT_FUNCTION) {
                    resolver_error(
                        resolver,
                        stmt->as.class_stmt.name,
                        "Class method must be a function declaration."
                    );
                    continue;
                }

                if (method->as.function.name.lexeme != NULL &&
                    strcmp(method->as.function.name.lexeme, "init") == 0) {
                    method_type = RESOLVER_FUNCTION_INITIALIZER;
                }

                resolver_resolve_function(resolver, method, method_type);
            }

            if (this_scope_started) {
                resolver_end_scope(resolver);
            }
            if (super_scope_started) {
                resolver_end_scope(resolver);
            }

            resolver->current_class = enclosing_class;
            break;
        }

        case STMT_EXPRESSION:
            resolver_resolve_expr(resolver, stmt->as.expression.expression);
            break;

        case STMT_FUNCTION:
            /* Define before resolving body, so recursion is supported. */
            (void)resolver_declare(resolver, stmt->as.function.name);
            (void)resolver_define(resolver, stmt->as.function.name);
            resolver_resolve_function(
                resolver,
                stmt,
                RESOLVER_FUNCTION_FUNCTION
            );
            break;

        case STMT_IF:
            resolver_resolve_expr(resolver, stmt->as.if_stmt.condition);
            resolver_resolve_stmt(resolver, stmt->as.if_stmt.then_branch);
            if (stmt->as.if_stmt.else_branch != NULL) {
                resolver_resolve_stmt(resolver, stmt->as.if_stmt.else_branch);
            }
            break;

        case STMT_PRINT:
            resolver_resolve_expr(resolver, stmt->as.print.expression);
            break;

        case STMT_RETURN:
            if (resolver->current_function == RESOLVER_FUNCTION_NONE) {
                resolver_error(
                    resolver,
                    stmt->as.return_stmt.keyword,
                    "Can't return from top-level code."
                );
                /* Match the C++ implementation: do not resolve a top-level return value. */
                break;
            }

            if (stmt->as.return_stmt.value != NULL) {
                if (resolver->current_function == RESOLVER_FUNCTION_INITIALIZER) {
                    resolver_error(
                        resolver,
                        stmt->as.return_stmt.keyword,
                        "Can't return a value from an initializer."
                    );
                }
                resolver_resolve_expr(resolver, stmt->as.return_stmt.value);
            }
            break;

        case STMT_VAR: {
            Token name = stmt->as.var.name;
            if (!resolver_declare(resolver, name)) {
                break;
            }
            if (stmt->as.var.initializer != NULL) {
                resolver_resolve_expr(resolver, stmt->as.var.initializer);
            }
            (void)resolver_define(resolver, name);
            break;
        }

        case STMT_WHILE:
            resolver_resolve_expr(resolver, stmt->as.while_stmt.condition);
            resolver_resolve_stmt(resolver, stmt->as.while_stmt.body);
            break;

        default:
            resolver_error(
                resolver,
                synthetic_token(TOKEN_ERROR, ""),
                "Unsupported statement node in current AST."
            );
            break;
    }
}

void resolver_resolve_statements(
    Resolver *resolver,
    Stmt *const *statements,
    size_t statement_count
)
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

/* ---------------------------------------------------------
 * Cleanup
 * --------------------------------------------------------- */

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
