#include "new_Resolver.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static bool reserveScopes(Resolver* resolver, size_t needed)
{
    if (needed <= resolver->scope_capacity) {
        return true;
    }
    size_t new_capacity = resolver->scope_capacity; 
    if (new_capacity == 0) {
        new_capacity = 4;
    }
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            return false;
        }
        new_capacity *= 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(Scope)) {
        return false;
    }
    Scope* new_scopes = realloc(resolver->scopes, new_capacity * sizeof(Scope));
    if (new_scopes == NULL) {
        return false;
    }
    resolver->scopes = new_scopes;
    resolver->scope_capacity = new_capacity;
    return true;
}

bool beginScope(Resolver* resolver)
{
 if (resolver == NULL) {
        return false;
    }
    if (!reserveScopes(resolver, resolver->scope_count + 1)) {
        resolver->out_of_memory = true;
        return false;
    }
    resolver->scopes[resolver->scope_count] = (Scope){0};
    resolver->scope_count++;
    return true;
}

void endScope(Resolver *resolver)
{
    if (resolver == NULL || resolver->scope_count == 0) {
        return;
    }

    Scope *scope =
        &resolver->scopes[resolver->scope_count - 1];

    free(scope->items);

    *scope = (Scope){0};

    resolver->scope_count--;
}

static bool reserveEntries(Scope *scope, size_t needed)
{
    if (needed <= scope->capacity) {
        return true;
    }

    size_t new_capacity = scope->capacity;

    if (new_capacity == 0) {
        new_capacity = 4;
    }

    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            return false;
        }

        new_capacity *= 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(ScopeEntry)) {
        return false;
    }

    ScopeEntry *new_items = realloc(
        scope->items,
        new_capacity * sizeof(ScopeEntry)
    );

    if (new_items == NULL) {
        return false;
    }

    scope->items = new_items;
    scope->capacity = new_capacity;

    return true;
}

bool declare(Resolver *resolver, Token name)
{
    if (resolver == NULL) {
        return false;
    }

    if (resolver->scope_count == 0) {
        return true;
    }

    Scope *scope =
        &resolver->scopes[resolver->scope_count - 1];

    const char *name_text =
        name.lexeme != NULL ? name.lexeme : "";

    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->items[i].name, name_text) == 0) {
            error(
                resolver,
                name,
                "Already a variable with this name in this scope."
            );

            scope->items[i].defined = false;

            return true;
        }
    }

    if (!reserveEntries(scope, scope->count + 1)) {
        resolver->out_of_memory = true;
        return false;
    }

    scope->items[scope->count].name = name_text;
    scope->items[scope->count].defined = false;
    scope->count++;

    return true;
}

bool define(Resolver *resolver, Token name)
{
    if (resolver == NULL) {
        return false;
    }

    if (resolver->scope_count == 0) {
        return true;
    }

    Scope *scope =
        &resolver->scopes[resolver->scope_count - 1];

    const char *name_text =
        name.lexeme != NULL ? name.lexeme : "";

    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->items[i].name, name_text) == 0) {
            scope->items[i].defined = true;
            return true;
        }
    }

    if (!reserveEntries(scope, scope->count + 1)) {
        resolver->out_of_memory = true;
        return false;
    }

    scope->items[scope->count].name = name_text;
    scope->items[scope->count].defined = true;
    scope->count++;

    return true;
}

void error(
    Resolver *resolver,
    Token name,
    const char *message
)
{
    if (resolver == NULL) {
        return;
    }

    resolver->had_error = true;

    const char *name_text =
        name.lexeme != NULL ? name.lexeme : "";

    const char *message_text =
        message != NULL ? message : "Unknown resolver error.";

    fprintf(
        stderr,
        "[line %d] Error at '%s': %s\n",
        name.line,
        name_text,
        message_text
    );
}

void resolver_init(Resolver *resolver)
{
    if (resolver == NULL) {
        return;
    }

    *resolver = (Resolver){0};
}

void resolver_free(Resolver *resolver)
{
    if (resolver == NULL) {
        return;
    }

    for (size_t i = 0; i < resolver->scope_count; i++) {
        free(resolver->scopes[i].items);
    }

    free(resolver->scopes);

    *resolver = (Resolver){0};
}