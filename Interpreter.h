#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <stdbool.h>
#include <stdio.h>
#include "AST.h"
#include "environment.h"

typedef enum {
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_NUMBER,
    VALUE_STRING
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        char *string;
    } as;
} Value;

typedef struct {
    Environment *globals;
    Environment *environment;
    FILE *out;
    FILE *err;
    bool had_error;
} Interpreter;

Value value_nil(void);
Value value_bool(bool value);
Value value_number(double value);
Value value_string(const char *value);
void value_free(Value *value);
char *value_to_string(Value value);
bool value_is_truthy(Value value);
bool value_is_equal(Value a, Value b);

void interpreter_init(Interpreter *interpreter, FILE *out, FILE *err);
void interpreter_free(Interpreter *interpreter);
bool interpreter_interpret(Interpreter *interpreter, Stmt *statements);
bool interpreter_evaluate(Interpreter *interpreter, Expr *expression);

#endif
