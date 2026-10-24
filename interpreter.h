#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "AST.h"
#include "environment.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RuntimeObject RuntimeObject;
typedef struct InterpreterEnvironmentNode InterpreterEnvironmentNode;

typedef enum {
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_FUNCTION,
    VALUE_CLASS,
    VALUE_INSTANCE
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
        char *string;
        RuntimeObject *object;
    } as;
} Value;

typedef struct {
    const Expr *expression;
    size_t distance;
} InterpreterLocal;

typedef struct Interpreter {
    Environment *globals;
    Environment *environment;

    FILE *out;
    FILE *err;

    InterpreterLocal *locals;
    size_t local_count;
    size_t local_capacity;

    RuntimeObject *objects;
    InterpreterEnvironmentNode *environments;

    Value return_value;
    bool returning;
    bool had_error;
    bool out_of_memory;
} Interpreter;

/* Values. Strings returned by value_string() are owned by the Value. */
Value value_nil(void);
Value value_bool(bool value);
Value value_number(double value);
Value value_string(const char *value);
Value value_clone(const Value *value);
void value_free(Value *value);
bool value_is_truthy(Value value);
bool value_is_equal(Value left, Value right);
char *value_to_string(Value value); /* Caller frees the returned string. */

/* Lifecycle. Initialize an Interpreter once before first use. */
bool interpreter_init(Interpreter *interpreter, FILE *out, FILE *err);
void interpreter_free(Interpreter *interpreter);

/* Interpret a parsed StmtList or an array of statements. */
bool interpreter_interpret(
    Interpreter *interpreter,
    Stmt *const *statements,
    size_t statement_count
);
bool interpreter_interpret_list(Interpreter *interpreter, const StmtList *statements);
/* Compatibility wrapper: evaluate then discard the result, like the earlier C API. */
bool interpreter_evaluate(Interpreter *interpreter, Expr *expression);
/* Evaluate and return an owned Value result. Caller must value_free(&result). */
bool interpreter_evaluate_value(Interpreter *interpreter, Expr *expression, Value *result);

/* Lexical resolution bridge used by Resolver. */
void interpreter_resolve(Interpreter *interpreter, const Expr *expression, size_t distance);
void interpreter_resolver_binding_sink(void *context, const Expr *expression, size_t distance);
bool interpreter_get_resolution(
    const Interpreter *interpreter,
    const Expr *expression,
    size_t *distance_out
);

#ifdef __cplusplus
}
#endif

#endif /* INTERPRETER_H */
