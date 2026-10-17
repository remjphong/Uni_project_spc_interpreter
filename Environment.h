#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct EnvironmentEntry {
    char *name;
    void *value;
    struct EnvironmentEntry *next;
} EnvironmentEntry;

typedef struct Environment {
    EnvironmentEntry *entries;
    struct Environment *enclosing;
} Environment;

Environment *environment_new(Environment *enclosing);
void environment_free(Environment *environment);
bool environment_define(Environment *environment, const char *name, void *value);
bool environment_assign(Environment *environment, const char *name, void *value);
void *environment_get(const Environment *environment, const char *name);

#endif
