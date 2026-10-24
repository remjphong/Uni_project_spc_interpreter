#include "environment.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
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

static EnvironmentEntry *find_entry(Environment *environment, const char *name)
{
    EnvironmentEntry *entry;

    if (environment == NULL || name == NULL) {
        return NULL;
    }

    for (entry = environment->entries; entry != NULL; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static const EnvironmentEntry *find_entry_const(
    const Environment *environment,
    const char *name
)
{
    const EnvironmentEntry *entry;

    if (environment == NULL || name == NULL) {
        return NULL;
    }

    for (entry = environment->entries; entry != NULL; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

Environment *environment_new(Environment *enclosing)
{
    Environment *environment = (Environment *)calloc(1, sizeof(*environment));

    if (environment == NULL) {
        return NULL;
    }

    if (enclosing != NULL && !environment_retain(enclosing)) {
        free(environment);
        return NULL;
    }

    environment->enclosing = enclosing;
    environment->entries = NULL;
    environment->reference_count = 1;
    return environment;
}

bool environment_retain(Environment *environment)
{
    if (environment == NULL || environment->reference_count == SIZE_MAX) {
        return false;
    }

    ++environment->reference_count;
    return true;
}

void environment_free(Environment *environment)
{
    /* Releasing a child also releases its retained parent, iteratively. */
    while (environment != NULL) {
        EnvironmentEntry *entry;
        Environment *parent;

        /* Defensive guard against malformed/double-released objects. */
        if (environment->reference_count == 0) {
            return;
        }

        --environment->reference_count;
        if (environment->reference_count != 0) {
            return;
        }

        parent = environment->enclosing;
        entry = environment->entries;

        while (entry != NULL) {
            EnvironmentEntry *next = entry->next;
            free(entry->name);
            /* Values are non-owning pointers and are not freed here. */
            free(entry);
            entry = next;
        }

        free(environment);
        environment = parent;
    }
}

bool environment_define(Environment *environment, const char *name, void *value)
{
    EnvironmentEntry *entry;
    char *name_copy;

    if (environment == NULL || name == NULL) {
        return false;
    }

    entry = find_entry(environment, name);
    if (entry != NULL) {
        /* Equivalent to values[name] = value in the C++ implementation. */
        entry->value = value;
        return true;
    }

    entry = (EnvironmentEntry *)calloc(1, sizeof(*entry));
    if (entry == NULL) {
        return false;
    }

    name_copy = copy_string(name);
    if (name_copy == NULL) {
        free(entry);
        return false;
    }

    entry->name = name_copy;
    entry->value = value;
    entry->next = environment->entries;
    environment->entries = entry;
    return true;
}

bool environment_get_checked(
    const Environment *environment,
    const char *name,
    void **value_out
)
{
    const Environment *current;

    if (name == NULL || value_out == NULL) {
        return false;
    }

    *value_out = NULL;
    for (current = environment; current != NULL; current = current->enclosing) {
        const EnvironmentEntry *entry = find_entry_const(current, name);
        if (entry != NULL) {
            *value_out = entry->value;
            return true;
        }
    }

    return false;
}

void *environment_get(const Environment *environment, const char *name)
{
    void *value = NULL;
    (void)environment_get_checked(environment, name, &value);
    return value;
}

bool environment_assign(Environment *environment, const char *name, void *value)
{
    Environment *current;

    if (name == NULL) {
        return false;
    }

    for (current = environment; current != NULL; current = current->enclosing) {
        EnvironmentEntry *entry = find_entry(current, name);
        if (entry != NULL) {
            entry->value = value;
            return true;
        }
    }

    /* C++ Environment::assign throws if the name is absent; false is the C status. */
    return false;
}

Environment *environment_ancestor(Environment *environment, int distance)
{
    int i;
    Environment *current = environment;

    if (distance < 0) {
        return NULL;
    }

    for (i = 0; i < distance; ++i) {
        if (current == NULL || current->enclosing == NULL) {
            return NULL;
        }
        current = current->enclosing;
    }

    return current;
}

bool environment_get_at(
    Environment *environment,
    int distance,
    const char *name,
    void **value_out
)
{
    Environment *ancestor;
    const EnvironmentEntry *entry;

    if (name == NULL || value_out == NULL) {
        return false;
    }

    *value_out = NULL;
    ancestor = environment_ancestor(environment, distance);
    if (ancestor == NULL) {
        return false;
    }

    entry = find_entry_const(ancestor, name);
    if (entry == NULL) {
        return false;
    }

    *value_out = entry->value;
    return true;
}

bool environment_assign_at(
    Environment *environment,
    int distance,
    const char *name,
    void *value
)
{
    Environment *ancestor = environment_ancestor(environment, distance);

    if (ancestor == NULL || name == NULL) {
        return false;
    }

    /* C++ assignAt writes ancestor(distance)->values[name] directly. */
    return environment_define(ancestor, name, value);
}

bool environment_format_undefined_error(
    char *buffer,
    size_t buffer_size,
    const char *name
)
{
    int written;

    if (buffer == NULL || buffer_size == 0 || name == NULL) {
        return false;
    }

    written = snprintf(buffer, buffer_size, "Undefined variable '%s'.", name);
    return written >= 0 && (size_t)written < buffer_size;
}
