#ifndef RESOLVER_H
#define RESOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "AST.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration: Resolver does not depend on Interpreter internals. */
typedef struct Interpreter Interpreter;

typedef struct {
    char *name;       /* Owned copy of the variable name. */
    bool defined;     /* false = declared, true = defined. */
} ScopeEntry;

typedef struct {
    ScopeEntry *entries;
    size_t count;
    size_t capacity;
} Scope;

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
    const Expr *expression;
    size_t distance;
} ResolverBinding;

/*
 * Optional sink used to mirror each local binding into Interpreter storage.
 * `context` can be the Interpreter pointer. The callback should copy/store the
 * expression pointer and lexical distance; it must not take ownership of AST.
 */
typedef void (*ResolverBindingSink)(
    void *context,
    const Expr *expression,
    size_t distance
);

typedef struct Resolver {
    Interpreter *interpreter; /* Kept for API compatibility; opaque here. */

    Scope *scopes;
    size_t scope_count;
    size_t scope_capacity;

    ResolverBinding *bindings;
    size_t binding_count;
    size_t binding_capacity;

    ResolverFunctionType current_function;
    ResolverClassType current_class;

    ResolverBindingSink binding_sink;
    void *binding_sink_context;
    FILE *error_stream;

    bool had_error;
    bool out_of_memory;
} Resolver;

/* Initialize once before the first use. Error output defaults to stderr. */
void resolver_init(Resolver *resolver, Interpreter *interpreter);
void resolver_free(Resolver *resolver);
void resolver_set_error_stream(Resolver *resolver, FILE *stream);
void resolver_set_binding_sink(
    Resolver *resolver,
    ResolverBindingSink sink,
    void *context
);

/* Resolve a whole program or one AST node. */
void resolver_resolve_statements(
    Resolver *resolver,
    Stmt *const *statements,
    size_t statement_count
);
void resolver_resolve_stmt(Resolver *resolver, Stmt *stmt);
void resolver_resolve_expr(Resolver *resolver, Expr *expr);
void resolver_resolve_local(Resolver *resolver, Expr *expr, Token name);

/* Scope operations, exposed for unit tests and future integrations. */
bool resolver_begin_scope(Resolver *resolver);
void resolver_end_scope(Resolver *resolver);
bool resolver_declare(Resolver *resolver, Token name);
bool resolver_define(Resolver *resolver, Token name);

/* Error reporting/status. */
void resolver_error(Resolver *resolver, Token token, const char *message);
bool resolver_had_error(const Resolver *resolver);
bool resolver_out_of_memory(const Resolver *resolver);

/*
 * Read local lexical distance. Returns false for globals or an unresolved node.
 * The Interpreter must use this data (or a binding sink) when looking up or
 * assigning variables. Distances are valid while the AST addresses stay valid.
 */
bool resolver_get_distance(
    const Resolver *resolver,
    const Expr *expression,
    size_t *distance_out
);

#ifdef __cplusplus
}
#endif

#endif /* RESOLVER_H */
