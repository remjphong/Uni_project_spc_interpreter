#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EnvironmentEntry EnvironmentEntry;
typedef struct Environment Environment;

/*
 * Environment values are opaque, non-owning pointers.
 * In this interpreter, pass pointers to runtime Value objects (or another
 * stable representation); this API does not copy or free the pointed-to value.
 */
struct EnvironmentEntry {
    char *name;
    void *value;
    EnvironmentEntry *next;
};

/*
 * `enclosing` is retained by environment_new(), mirroring the shared ownership
 * of std::shared_ptr<Environment> in the C++ version.
 * Environment objects returned by environment_new() start with one reference.
 */
struct Environment {
    Environment *enclosing;
    EnvironmentEntry *entries;
    size_t reference_count;
};

/* Create an environment. `enclosing` may be NULL. Returns NULL on allocation failure. */
Environment *environment_new(Environment *enclosing);

/* Explicitly retain a reference. Returns false for NULL or reference-count overflow. */
bool environment_retain(Environment *environment);

/* Release one reference; a parent is released when its child is destroyed. */
void environment_free(Environment *environment);

/* Define or replace a binding in this environment only. */
bool environment_define(Environment *environment, const char *name, void *value);

/*
 * Look up from this environment outward through enclosing environments.
 * Returns true if found, even when the stored value is NULL. `value_out` is required.
 */
bool environment_get_checked(
    const Environment *environment,
    const char *name,
    void **value_out
);

/* Backward-compatible convenience API; NULL means missing OR a stored NULL. */
void *environment_get(const Environment *environment, const char *name);

/* Assign the nearest existing binding; does not create an undefined variable. */
bool environment_assign(Environment *environment, const char *name, void *value);

/* Return a borrowed pointer to the ancestor at `distance`; negative/out-of-range => NULL. */
Environment *environment_ancestor(Environment *environment, int distance);

/*
 * Exact-scope lookup at a lexical distance. Unlike environment_get(), this does
 * not continue searching from the ancestor into its own parents.
 */
bool environment_get_at(
    Environment *environment,
    int distance,
    const char *name,
    void **value_out
);

/*
 * Match the C++ assignAt behavior: assign/create the binding in the exact ancestor
 * selected by `distance`, without searching other scopes.
 */
bool environment_assign_at(
    Environment *environment,
    int distance,
    const char *name,
    void *value
);

/* Optional helper for creating the same error text used by the C++ implementation. */
bool environment_format_undefined_error(
    char *buffer,
    size_t buffer_size,
    const char *name
);

#ifdef __cplusplus
}
#endif

#endif /* ENVIRONMENT_H */
