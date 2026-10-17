#include "environment.h"
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *copy = (char *)malloc(n);
    if (copy) memcpy(copy, s, n);
    return copy;
}

Environment *environment_new(Environment *enclosing) {
    Environment *env = (Environment *)calloc(1, sizeof(*env));
    if (env) env->enclosing = enclosing;
    return env;
}

void environment_free(Environment *environment) {
    if (!environment) return;
    EnvironmentEntry *entry = environment->entries;
    while (entry) {
        EnvironmentEntry *next = entry->next;
        free(entry->name);
        
        free(entry);
        entry = next;
    }
    free(environment);
}

bool environment_define(Environment *environment, const char *name, void *value) {
    if (!environment || !name) return false;

    for (EnvironmentEntry *e = environment->entries; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            e->value = value;
            return true;
        }
    }

    EnvironmentEntry *entry = (EnvironmentEntry *)calloc(1, sizeof(*entry));
    if (!entry) return false;
    entry->name = copy_string(name);
    if (!entry->name) {
        free(entry);
        return false;
    }
    entry->value = value;
    entry->next = environment->entries;
    environment->entries = entry;
    return true;
}

bool environment_assign(Environment *environment, const char *name, void *value) {
    if (!environment || !name) return false;
    for (Environment *env = environment; env; env = env->enclosing) {
        for (EnvironmentEntry *e = env->entries; e; e = e->next) {
            if (strcmp(e->name, name) == 0) {
                e->value = value;
                return true;
            }
        }
    }
    return false;
}

void *environment_get(const Environment *environment, const char *name) {
    if (!name) return NULL;
    for (const Environment *env = environment; env; env = env->enclosing) {
        for (EnvironmentEntry *e = env->entries; e; e = e->next) {
            if (strcmp(e->name, name) == 0) return e->value;
        }
    }
    return NULL;
}
