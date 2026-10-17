#ifndef RESOLVER_H
#define RESOLVER_H

#include <stdbool.h>
#include <stddef.h>

#include "AST.h"

// khai báo trước interpreter
typedef struct Interpreter Interpreter;

// function state
typedef enum {
    RESOLVER_FUNCTION_NONE,
    RESOLVER_FUNCTION_FUNCTION,
    RESOLVER_FUNCTION_INITIALIZER,
    RESOLVER_FUNCTION_METHOD
} ResolverFunctionType;

// class state
typedef enum {
    RESOLVER_CLASS_NONE,
    RESOLVER_CLASS_CLASS,
    RESOLVER_CLASS_SUBCLASS
} ResolverClassType;
// scope
typedef struct {
    const char* name;
    bool defined;
} ScopeEntry;
typedef struct {
    ScopeEntry* entries;
    size_t count;
    size_t capacity;
} Scope;
// resolver
typedef struct Resolver {
    Interpreter* interpreter;
    Scope* scopes;
    size_t scope_count;
    size_t scope_capacity;
    ResolverFunctionType current_function;
    ResolverClassType current_class;
    bool had_error;
    bool out_of_memory;
} Resolver; 

// public api
void resolver_init(Resolver* resolver, Interpreter* interpreter);
void resolver_free(Resolver* resolver);
void resolver_resolve_statements(Resolver* resolver, const Stmt** statements, size_t statement_count);
void resolver_resolve_stmt(Resolver* resolver, Stmt* stmt);
void resolver_resolve_expr(Resolver* resolver, Expr* expr);




#endif // RESOLVER_H
