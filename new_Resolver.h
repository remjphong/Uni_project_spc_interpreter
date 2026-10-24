#ifndef RESOLVER_H
#define RESOLVER_H

#include <stdbool.h>
#include <stddef.h>

#include "AST.h"
#include "interpreter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RESOLVER_FUNCTION_NONE,
    RESOLVER_FUNCTION_FUNCTION,
    RESOLVER_FUNCTION_INITIALIZER,
    RESOLVER_FUNCTION_METHOD
} ResolverFunctionType;

typedef enum {
    RESOLVER_CLASS_NONE,
    RESOLVER_CLASS_CLASS,
    RESOLVER_CLASS_SUBCLASS
} ResolverClassType;

typedef struct {
    char *name;       /* Name owned by this entry. */
    bool defined;     /* false after declare; true after define. */
} ScopeEntry;

typedef struct {
    ScopeEntry *entries;
    size_t count;
    size_t capacity;
} Scope;

/* Resolver's equivalent of Interpreter.locals: expression -> scope distance.
 * Keeping this map here avoids requiring interpreter_resolve() before the
 * current Interpreter implementation supports lexical scopes. */
typedef struct {
    const Expr *expression;
    size_t distance;
} ResolverBinding;

typedef struct Resolver {
    Interpreter *interpreter; /* Not owned; used for the error output stream. */

    Scope *scopes;
    size_t scope_count;
    size_t scope_capacity;

    ResolverBinding *bindings;
    size_t binding_count;
    size_t binding_capacity;

    ResolverFunctionType current_function;
    ResolverClassType current_class;

    bool had_error;
    bool out_of_memory;
} Resolver;

/* Initialize/reset a Resolver. The Interpreter, if provided, must outlive it. */
void resolver_init(Resolver *resolver, Interpreter *interpreter);

/* Free all memory owned by the Resolver. Does not free the Interpreter or AST. */
void resolver_free(Resolver *resolver);

/* Resolve Parser's statement array: resolver_resolve_statements(r, list.items,
 * list.count). This deliberately does not assume Stmt has a `next` pointer. */
void resolver_resolve_statements(
    Resolver *resolver,
    Stmt *const *statements,
    size_t statement_count
);

void resolver_resolve_stmt(Resolver *resolver, Stmt *stmt);
void resolver_resolve_expr(Resolver *resolver, Expr *expr);

/* Scope operations are exposed for unit tests and future Block/Function nodes. */
bool resolver_begin_scope(Resolver *resolver);
void resolver_end_scope(Resolver *resolver);
bool resolver_declare(Resolver *resolver, Token name);
bool resolver_define(Resolver *resolver, Token name);

/* Resolve a variable name against active local scopes. Names not found in a
 * local scope are treated as globals, matching the original C++ Resolver. */
void resolver_resolve_local(Resolver *resolver, Expr *expr, Token name);

/* Read a lexical distance produced by resolver_resolve_local(). Returns false
 * when expression resolved as a global or has not been resolved. */
bool resolver_get_distance(
    const Resolver *resolver,
    const Expr *expression,
    size_t *distance_out
);

void resolver_error(Resolver *resolver, Token token, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* RESOLVER_H */
