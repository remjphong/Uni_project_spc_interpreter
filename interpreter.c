#include "interpreter.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Runtime objects live in an interpreter-owned arena. Environment bindings
 * contain Value*; object values are non-owning pointers into this arena. */
typedef enum {
    OBJECT_FUNCTION,
    OBJECT_CLASS,
    OBJECT_INSTANCE
} RuntimeObjectType;

typedef struct MethodEntry {
    char *name;
    RuntimeObject *function;
    struct MethodEntry *next;
} MethodEntry;

typedef struct FieldEntry {
    char *name;
    Value value;
    struct FieldEntry *next;
} FieldEntry;

struct RuntimeObject {
    RuntimeObjectType type;
    RuntimeObject *next;
    union {
        struct {
            const Stmt *declaration; /* STMT_FUNCTION; AST must outlive execution. */
            Environment *closure;
            bool initializer;
        } function;
        struct {
            char *name;
            RuntimeObject *superclass;
            MethodEntry *methods;
        } klass;
        struct {
            RuntimeObject *klass;
            FieldEntry *fields;
        } instance;
    } as;
};

struct InterpreterEnvironmentNode {
    Environment *environment;
    InterpreterEnvironmentNode *next; /* Newest environment first. */
};

static char *copy_text(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        text = "";
    }
    length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = (char *)malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static FILE *error_stream(const Interpreter *interpreter)
{
    return interpreter != NULL && interpreter->err != NULL
        ? interpreter->err : stderr;
}

static void runtime_error(Interpreter *interpreter, const char *format, ...)
{
    va_list args;
    if (interpreter == NULL) {
        return;
    }
    interpreter->had_error = true;
    fputs("Runtime error: ", error_stream(interpreter));
    va_start(args, format);
    vfprintf(error_stream(interpreter), format, args);
    va_end(args);
    fputc('\n', error_stream(interpreter));
}

static void memory_error(Interpreter *interpreter)
{
    if (interpreter == NULL) {
        return;
    }
    interpreter->had_error = true;
    interpreter->out_of_memory = true;
    fputs("Runtime error: not enough memory.\n", error_stream(interpreter));
}

Value value_nil(void)
{
    Value value = {0};
    value.type = VALUE_NIL;
    return value;
}

Value value_bool(bool boolean)
{
    Value value = {0};
    value.type = VALUE_BOOL;
    value.as.boolean = boolean;
    return value;
}

Value value_number(double number)
{
    Value value = {0};
    value.type = VALUE_NUMBER;
    value.as.number = number;
    return value;
}

Value value_string(const char *text)
{
    Value value = {0};
    value.type = VALUE_STRING;
    value.as.string = copy_text(text);
    if (value.as.string == NULL) {
        value.type = VALUE_NIL;
    }
    return value;
}

void value_free(Value *value)
{
    if (value == NULL) {
        return;
    }
    if (value->type == VALUE_STRING) {
        free(value->as.string);
    }
    *value = value_nil();
}

static bool clone_value(Interpreter *interpreter, const Value *source, Value *out)
{
    if (source == NULL || out == NULL) {
        runtime_error(interpreter, "invalid value.");
        return false;
    }
    *out = *source;
    if (source->type == VALUE_STRING) {
        out->as.string = copy_text(source->as.string);
        if (out->as.string == NULL) {
            *out = value_nil();
            memory_error(interpreter);
            return false;
        }
    }
    return true;
}

Value value_clone(const Value *source)
{
    Value result = value_nil();
    if (source == NULL) {
        return result;
    }
    result = *source;
    if (source->type == VALUE_STRING) {
        result.as.string = copy_text(source->as.string);
        if (result.as.string == NULL) {
            result = value_nil();
        }
    }
    return result;
}

bool value_is_truthy(Value value)
{
    if (value.type == VALUE_NIL) {
        return false;
    }
    if (value.type == VALUE_BOOL) {
        return value.as.boolean;
    }
    return true;
}

static bool is_callable_type(ValueType type)
{
    return type == VALUE_FUNCTION || type == VALUE_CLASS;
}

bool value_is_equal(Value left, Value right)
{
    if (is_callable_type(left.type) && is_callable_type(right.type)) {
        return left.as.object == right.as.object;
    }
    if (left.type != right.type) {
        return false;
    }
    switch (left.type) {
        case VALUE_NIL:
            return true;
        case VALUE_BOOL:
            return left.as.boolean == right.as.boolean;
        case VALUE_NUMBER:
            return left.as.number == right.as.number;
        case VALUE_STRING:
            return strcmp(left.as.string != NULL ? left.as.string : "",
                          right.as.string != NULL ? right.as.string : "") == 0;
        case VALUE_FUNCTION:
        case VALUE_CLASS:
        case VALUE_INSTANCE:
            return left.as.object == right.as.object;
    }
    return false;
}

static RuntimeObject *find_method(RuntimeObject *klass, const char *name)
{
    RuntimeObject *current = klass;
    while (current != NULL && current->type == OBJECT_CLASS) {
        MethodEntry *method;
        for (method = current->as.klass.methods; method != NULL; method = method->next) {
            if (strcmp(method->name, name != NULL ? name : "") == 0) {
                return method->function;
            }
        }
        current = current->as.klass.superclass;
    }
    return NULL;
}

static char *object_to_string(const RuntimeObject *object)
{
    if (object == NULL) {
        return copy_text("nil");
    }
    switch (object->type) {
        case OBJECT_FUNCTION: {
            const Stmt *declaration = object->as.function.declaration;
            const char *name = declaration != NULL && declaration->type == STMT_FUNCTION
                ? declaration->as.function.name.lexeme : "";
            size_t n = strlen(name != NULL ? name : "") + 6;
            char *result = (char *)malloc(n);
            if (result != NULL) {
                (void)snprintf(result, n, "<fn %s>", name != NULL ? name : "");
            }
            return result;
        }
        case OBJECT_CLASS:
            return copy_text(object->as.klass.name);
        case OBJECT_INSTANCE: {
            const RuntimeObject *klass = object->as.instance.klass;
            const char *name = klass != NULL && klass->type == OBJECT_CLASS
                ? klass->as.klass.name : "<class>";
            size_t n = strlen(name) + 12;
            char *result = (char *)malloc(n);
            if (result != NULL) {
                (void)snprintf(result, n, "<%s instance>", name);
            }
            return result;
        }
    }
    return copy_text("<object>");
}

char *value_to_string(Value value)
{
    char buffer[128];
    switch (value.type) {
        case VALUE_NIL:
            return copy_text("nil");
        case VALUE_BOOL:
            return copy_text(value.as.boolean ? "true" : "false");
        case VALUE_NUMBER:
            if (isfinite(value.as.number) && floor(value.as.number) == value.as.number &&
                value.as.number >= (double)LLONG_MIN &&
                value.as.number < (double)LLONG_MAX) {
                (void)snprintf(buffer, sizeof(buffer), "%lld",
                               (long long)value.as.number);
            } else {
                size_t length;
                (void)snprintf(buffer, sizeof(buffer), "%.6f", value.as.number);
                length = strlen(buffer);
                while (length > 0 && buffer[length - 1] == '0') {
                    buffer[--length] = '\0';
                }
                if (length > 0 && buffer[length - 1] == '.') {
                    buffer[--length] = '\0';
                }
            }
            return copy_text(buffer);
        case VALUE_STRING:
            return copy_text(value.as.string);
        case VALUE_FUNCTION:
        case VALUE_CLASS:
        case VALUE_INSTANCE:
            return object_to_string(value.as.object);
    }
    return copy_text("<object>");
}

static RuntimeObject *new_object(Interpreter *interpreter, RuntimeObjectType type)
{
    RuntimeObject *object;
    if (interpreter == NULL) {
        return NULL;
    }
    object = (RuntimeObject *)calloc(1, sizeof(*object));
    if (object == NULL) {
        memory_error(interpreter);
        return NULL;
    }
    object->type = type;
    object->next = interpreter->objects;
    interpreter->objects = object;
    return object;
}

static Environment *new_environment(Interpreter *interpreter, Environment *enclosing)
{
    Environment *environment;
    InterpreterEnvironmentNode *node;

    environment = environment_new(enclosing);
    if (environment == NULL) {
        memory_error(interpreter);
        return NULL;
    }
    node = (InterpreterEnvironmentNode *)malloc(sizeof(*node));
    if (node == NULL) {
        environment_free(environment);
        memory_error(interpreter);
        return NULL;
    }
    node->environment = environment;
    node->next = interpreter->environments;
    interpreter->environments = node;
    return environment;
}

static EnvironmentEntry *find_direct_entry(Environment *environment, const char *name)
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

static Value *new_stored_value(Interpreter *interpreter, const Value *value)
{
    Value *stored = (Value *)malloc(sizeof(*stored));
    if (stored == NULL) {
        memory_error(interpreter);
        return NULL;
    }
    if (!clone_value(interpreter, value, stored)) {
        free(stored);
        return NULL;
    }
    return stored;
}

static bool define_value_in_environment(
    Interpreter *interpreter,
    Environment *environment,
    const char *name,
    const Value *value
)
{
    EnvironmentEntry *existing;
    Value *stored;

    if (interpreter == NULL || environment == NULL || name == NULL || value == NULL) {
        return false;
    }
    stored = new_stored_value(interpreter, value);
    if (stored == NULL) {
        return false;
    }
    existing = find_direct_entry(environment, name);
    if (existing != NULL) {
        Value *old = (Value *)existing->value;
        if (old != NULL) {
            value_free(old);
            free(old);
        }
        existing->value = stored;
        return true;
    }
    if (!environment_define(environment, name, stored)) {
        value_free(stored);
        free(stored);
        memory_error(interpreter);
        return false;
    }
    return true;
}

static bool assign_value_at(
    Interpreter *interpreter,
    Environment *environment,
    int distance,
    const char *name,
    const Value *value
)
{
    Environment *target;
    EnvironmentEntry *existing;
    Value *stored;

    target = environment_ancestor(environment, distance);
    if (target == NULL || name == NULL || value == NULL) {
        runtime_error(interpreter, "Unable to access resolved variable '%s'.",
                      name != NULL ? name : "");
        return false;
    }
    stored = new_stored_value(interpreter, value);
    if (stored == NULL) {
        return false;
    }
    existing = find_direct_entry(target, name);
    if (existing != NULL) {
        Value *old = (Value *)existing->value;
        if (old != NULL) {
            value_free(old);
            free(old);
        }
        existing->value = stored;
        return true;
    }
    if (!environment_assign_at(environment, distance, name, stored)) {
        value_free(stored);
        free(stored);
        runtime_error(interpreter, "Undefined variable '%s'.", name);
        return false;
    }
    return true;
}

static bool read_value_at(
    Interpreter *interpreter,
    Environment *environment,
    int distance,
    const char *name,
    Value *result
)
{
    void *stored_pointer = NULL;
    Value *stored;
    if (!environment_get_at(environment, distance, name, &stored_pointer) ||
        stored_pointer == NULL) {
        runtime_error(interpreter, "Undefined variable '%s'.", name != NULL ? name : "");
        return false;
    }
    stored = (Value *)stored_pointer;
    return clone_value(interpreter, stored, result);
}

static bool read_global_value(
    Interpreter *interpreter,
    const char *name,
    Value *result
)
{
    void *stored_pointer = NULL;
    if (!environment_get_checked(interpreter->globals, name, &stored_pointer) ||
        stored_pointer == NULL) {
        runtime_error(interpreter, "Undefined variable '%s'.", name != NULL ? name : "");
        return false;
    }
    return clone_value(interpreter, (const Value *)stored_pointer, result);
}

static bool lookup_variable(
    Interpreter *interpreter,
    const Token *name,
    const Expr *expression,
    Value *result
)
{
    size_t distance;
    if (interpreter_get_resolution(interpreter, expression, &distance)) {
        if (distance > (size_t)INT_MAX) {
            runtime_error(interpreter, "Resolved scope distance is too large.");
            return false;
        }
        return read_value_at(interpreter, interpreter->environment,
                             (int)distance, name->lexeme, result);
    }
    return read_global_value(interpreter, name->lexeme, result);
}

static bool assign_variable(
    Interpreter *interpreter,
    const Token *name,
    const Expr *expression,
    const Value *value
)
{
    size_t distance;
    if (interpreter_get_resolution(interpreter, expression, &distance)) {
        if (distance > (size_t)INT_MAX) {
            runtime_error(interpreter, "Resolved scope distance is too large.");
            return false;
        }
        return assign_value_at(interpreter, interpreter->environment,
                               (int)distance, name->lexeme, value);
    }

    /* C++ uses globals->assign(), which does not create a missing global. */
    if (find_direct_entry(interpreter->globals, name->lexeme) == NULL) {
        runtime_error(interpreter, "Undefined variable '%s'.", name->lexeme);
        return false;
    }
    return assign_value_at(interpreter, interpreter->globals, 0, name->lexeme, value);
}

static bool set_instance_field(
    Interpreter *interpreter,
    RuntimeObject *instance,
    const char *name,
    const Value *value
)
{
    FieldEntry *field;
    Value copy;
    if (instance == NULL || instance->type != OBJECT_INSTANCE || name == NULL) {
        runtime_error(interpreter, "Only instances have fields.");
        return false;
    }
    for (field = instance->as.instance.fields; field != NULL; field = field->next) {
        if (strcmp(field->name, name) == 0) {
            if (!clone_value(interpreter, value, &copy)) {
                return false;
            }
            value_free(&field->value);
            field->value = copy;
            return true;
        }
    }
    field = (FieldEntry *)calloc(1, sizeof(*field));
    if (field == NULL) {
        memory_error(interpreter);
        return false;
    }
    field->name = copy_text(name);
    if (field->name == NULL || !clone_value(interpreter, value, &copy)) {
        free(field->name);
        free(field);
        if (!interpreter->out_of_memory) {
            memory_error(interpreter);
        }
        return false;
    }
    field->value = copy;
    field->next = instance->as.instance.fields;
    instance->as.instance.fields = field;
    return true;
}

static bool get_instance_property(
    Interpreter *interpreter,
    RuntimeObject *instance,
    const Token *name,
    Value *result
);

static bool execute_statement(Interpreter *interpreter, Stmt *statement);
static bool evaluate_expression(Interpreter *interpreter, Expr *expression, Value *result);
static bool execute_statement_list(
    Interpreter *interpreter,
    Stmt *const *statements,
    size_t count,
    bool stop_on_error
);

static RuntimeObject *new_function(
    Interpreter *interpreter,
    const Stmt *declaration,
    Environment *closure,
    bool initializer
)
{
    RuntimeObject *function;
    if (declaration == NULL || declaration->type != STMT_FUNCTION) {
        runtime_error(interpreter, "Invalid function declaration.");
        return NULL;
    }
    function = new_object(interpreter, OBJECT_FUNCTION);
    if (function == NULL) {
        return NULL;
    }
    function->as.function.declaration = declaration;
    function->as.function.closure = closure;
    function->as.function.initializer = initializer;
    return function;
}

static RuntimeObject *bind_method(
    Interpreter *interpreter,
    RuntimeObject *function,
    RuntimeObject *instance
)
{
    Environment *bound_environment;
    RuntimeObject *bound_function;
    Value this_value;

    if (function == NULL || function->type != OBJECT_FUNCTION ||
        instance == NULL || instance->type != OBJECT_INSTANCE) {
        runtime_error(interpreter, "Invalid method binding.");
        return NULL;
    }
    bound_environment = new_environment(interpreter, function->as.function.closure);
    if (bound_environment == NULL) {
        return NULL;
    }
    this_value = value_nil();
    this_value.type = VALUE_INSTANCE;
    this_value.as.object = instance;
    if (!define_value_in_environment(interpreter, bound_environment, "this", &this_value)) {
        return NULL;
    }
    bound_function = new_function(interpreter,
                                  function->as.function.declaration,
                                  bound_environment,
                                  function->as.function.initializer);
    return bound_function;
}

static bool call_function(
    Interpreter *interpreter,
    RuntimeObject *function,
    const Value *arguments,
    size_t argument_count,
    Value *result
)
{
    const Stmt *declaration;
    Environment *function_environment;
    Environment *previous_environment;
    Value previous_return;
    bool previous_returning;
    bool returned;
    bool ok = true;
    size_t i;
    Value return_result = value_nil();

    if (function == NULL || function->type != OBJECT_FUNCTION) {
        runtime_error(interpreter, "Can only call functions and classes.");
        return false;
    }
    declaration = function->as.function.declaration;
    if (declaration->as.function.param_count != argument_count) {
        runtime_error(interpreter, "Expected %zu arguments but got %zu.",
                      declaration->as.function.param_count, argument_count);
        return false;
    }
    function_environment = new_environment(interpreter, function->as.function.closure);
    if (function_environment == NULL) {
        return false;
    }
    for (i = 0; i < argument_count; ++i) {
        if (!define_value_in_environment(interpreter, function_environment,
                                         declaration->as.function.params[i].lexeme,
                                         &arguments[i])) {
            return false;
        }
    }

    previous_environment = interpreter->environment;
    previous_returning = interpreter->returning;
    previous_return = interpreter->return_value; /* Move-save owned return payload. */
    interpreter->return_value = value_nil();
    interpreter->returning = false;
    interpreter->environment = function_environment;

    ok = execute_statement_list(interpreter,
                                declaration->as.function.body.items,
                                declaration->as.function.body.count,
                                true);

    interpreter->environment = previous_environment;
    returned = interpreter->returning;
    return_result = interpreter->return_value;
    interpreter->return_value = previous_return;
    interpreter->returning = previous_returning;

    if (!ok) {
        value_free(&return_result);
        return false;
    }

    if (function->as.function.initializer) {
        Value this_value = value_nil();
        void *stored = NULL;
        if (!environment_get_checked(function->as.function.closure, "this", &stored) ||
            stored == NULL || !clone_value(interpreter, (Value *)stored, &this_value)) {
            value_free(&return_result);
            runtime_error(interpreter, "Invalid initializer receiver.");
            return false;
        }
        value_free(&return_result);
        *result = this_value;
        return true;
    }

    *result = returned ? return_result : value_nil();
    if (!returned) {
        value_free(&return_result);
    }
    return true;
}

static bool call_class(
    Interpreter *interpreter,
    RuntimeObject *klass,
    const Value *arguments,
    size_t argument_count,
    Value *result
)
{
    RuntimeObject *instance;
    RuntimeObject *initializer;
    if (klass == NULL || klass->type != OBJECT_CLASS) {
        runtime_error(interpreter, "Can only call functions and classes.");
        return false;
    }
    initializer = find_method(klass, "init");
    if (initializer != NULL) {
        size_t expected = initializer->as.function.declaration->as.function.param_count;
        if (expected != argument_count) {
            runtime_error(interpreter, "Expected %zu arguments but got %zu.",
                          expected, argument_count);
            return false;
        }
    } else if (argument_count != 0) {
        runtime_error(interpreter, "Expected 0 arguments but got %zu.", argument_count);
        return false;
    }
    instance = new_object(interpreter, OBJECT_INSTANCE);
    if (instance == NULL) {
        return false;
    }
    instance->as.instance.klass = klass;
    if (initializer != NULL) {
        RuntimeObject *bound = bind_method(interpreter, initializer, instance);
        Value initializer_result = value_nil();
        if (bound == NULL || !call_function(interpreter, bound, arguments,
                                            argument_count, &initializer_result)) {
            value_free(&initializer_result);
            return false;
        }
        value_free(&initializer_result);
    }
    result->type = VALUE_INSTANCE;
    result->as.object = instance;
    return true;
}

static bool call_callable(
    Interpreter *interpreter,
    Value callee,
    const Value *arguments,
    size_t argument_count,
    Value *result
)
{
    if (callee.type == VALUE_FUNCTION) {
        return call_function(interpreter, callee.as.object, arguments,
                             argument_count, result);
    }
    if (callee.type == VALUE_CLASS) {
        return call_class(interpreter, callee.as.object, arguments,
                          argument_count, result);
    }
    runtime_error(interpreter, "Can only call functions and classes.");
    return false;
}

static bool get_instance_property(
    Interpreter *interpreter,
    RuntimeObject *instance,
    const Token *name,
    Value *result
)
{
    FieldEntry *field;
    RuntimeObject *method;
    if (instance == NULL || instance->type != OBJECT_INSTANCE) {
        runtime_error(interpreter, "Only instances have properties.");
        return false;
    }
    for (field = instance->as.instance.fields; field != NULL; field = field->next) {
        if (strcmp(field->name, name->lexeme != NULL ? name->lexeme : "") == 0) {
            return clone_value(interpreter, &field->value, result);
        }
    }
    method = find_method(instance->as.instance.klass, name->lexeme);
    if (method != NULL) {
        RuntimeObject *bound = bind_method(interpreter, method, instance);
        if (bound == NULL) {
            return false;
        }
        result->type = VALUE_FUNCTION;
        result->as.object = bound;
        return true;
    }
    runtime_error(interpreter, "Undefined property '%s'.",
                  name->lexeme != NULL ? name->lexeme : "");
    return false;
}

bool interpreter_init(Interpreter *interpreter, FILE *out, FILE *err)
{
    if (interpreter == NULL) {
        return false;
    }
    *interpreter = (Interpreter){0};
    interpreter->out = out != NULL ? out : stdout;
    interpreter->err = err != NULL ? err : stderr;
    interpreter->return_value = value_nil();
    interpreter->globals = new_environment(interpreter, NULL);
    if (interpreter->globals == NULL) {
        interpreter->had_error = true;
        interpreter->out_of_memory = true;
        return false;
    }
    interpreter->environment = interpreter->globals;
    return true;
}

void interpreter_resolve(Interpreter *interpreter, const Expr *expression, size_t distance)
{
    size_t i;
    if (interpreter == NULL || expression == NULL) {
        return;
    }
    for (i = 0; i < interpreter->local_count; ++i) {
        if (interpreter->locals[i].expression == expression) {
            interpreter->locals[i].distance = distance;
            return;
        }
    }
    if (interpreter->local_count == interpreter->local_capacity) {
        size_t new_capacity = interpreter->local_capacity == 0 ? 16 : interpreter->local_capacity * 2;
        InterpreterLocal *new_locals;
        if (new_capacity < interpreter->local_capacity ||
            new_capacity > SIZE_MAX / sizeof(*new_locals)) {
            memory_error(interpreter);
            return;
        }
        new_locals = (InterpreterLocal *)realloc(
            interpreter->locals, new_capacity * sizeof(*new_locals));
        if (new_locals == NULL) {
            memory_error(interpreter);
            return;
        }
        interpreter->locals = new_locals;
        interpreter->local_capacity = new_capacity;
    }
    interpreter->locals[interpreter->local_count].expression = expression;
    interpreter->locals[interpreter->local_count].distance = distance;
    interpreter->local_count++;
}

void interpreter_resolver_binding_sink(void *context, const Expr *expression, size_t distance)
{
    interpreter_resolve((Interpreter *)context, expression, distance);
}

bool interpreter_get_resolution(
    const Interpreter *interpreter,
    const Expr *expression,
    size_t *distance_out
)
{
    size_t i;
    if (interpreter == NULL || expression == NULL || distance_out == NULL) {
        return false;
    }
    for (i = 0; i < interpreter->local_count; ++i) {
        if (interpreter->locals[i].expression == expression) {
            *distance_out = interpreter->locals[i].distance;
            return true;
        }
    }
    return false;
}

static bool evaluate_expression(Interpreter *interpreter, Expr *expression, Value *result)
{
    if (result != NULL) {
        *result = value_nil();
    }
    if (interpreter == NULL || expression == NULL || result == NULL) {
        runtime_error(interpreter, "Invalid expression.");
        return false;
    }

    switch (expression->type) {
        case EXPR_LITERAL:
            switch (expression->as.literal.value.type) {
                case LIT_NIL:
                    *result = value_nil();
                    return true;
                case LIT_BOOL:
                    *result = value_bool(expression->as.literal.value.as.boolean);
                    return true;
                case LIT_NUMBER:
                    *result = value_number(expression->as.literal.value.as.number);
                    return true;
                case LIT_STRING:
                    result->type = VALUE_STRING;
                    result->as.string = copy_text(expression->as.literal.value.as.string);
                    if (result->as.string == NULL) {
                        memory_error(interpreter);
                        return false;
                    }
                    return true;
            }
            runtime_error(interpreter, "Invalid literal.");
            return false;

        case EXPR_GROUPING:
            return evaluate_expression(interpreter,
                                      expression->as.grouping.expression, result);

        case EXPR_VARIABLE:
            return lookup_variable(interpreter, &expression->as.variable.name,
                                   expression, result);

        case EXPR_THIS:
            return lookup_variable(interpreter, &expression->as.this_expr.keyword,
                                   expression, result);

        case EXPR_ASSIGN: {
            Value value = value_nil();
            if (!evaluate_expression(interpreter, expression->as.assign.value, &value)) {
                return false;
            }
            if (!assign_variable(interpreter, &expression->as.assign.name,
                                 expression, &value)) {
                value_free(&value);
                return false;
            }
            *result = value;
            return true;
        }

        case EXPR_UNARY: {
            Value right = value_nil();
            TokenType op = expression->as.unary.op.type;
            if (!evaluate_expression(interpreter, expression->as.unary.right, &right)) {
                return false;
            }
            if (op == MINUS) {
                if (right.type != VALUE_NUMBER) {
                    value_free(&right);
                    runtime_error(interpreter, "Operand must be a number.");
                    return false;
                }
                *result = value_number(-right.as.number);
            } else if (op == BANG) {
                *result = value_bool(!value_is_truthy(right));
            } else {
                value_free(&right);
                runtime_error(interpreter, "Unsupported unary operator.");
                return false;
            }
            value_free(&right);
            return true;
        }

        case EXPR_BINARY: {
            Value left = value_nil();
            Value right = value_nil();
            TokenType op = expression->as.binary.op.type;
            double a, b;
            if (!evaluate_expression(interpreter, expression->as.binary.left, &left)) {
                return false;
            }
            if (!evaluate_expression(interpreter, expression->as.binary.right, &right)) {
                value_free(&left);
                return false;
            }
            if (op == EQUAL_EQUAL || op == BANG_EQUAL) {
                bool equal = value_is_equal(left, right);
                *result = value_bool(op == EQUAL_EQUAL ? equal : !equal);
            } else if (op == PLUS) {
                if (left.type == VALUE_NUMBER && right.type == VALUE_NUMBER) {
                    *result = value_number(left.as.number + right.as.number);
                } else if (left.type == VALUE_STRING && right.type == VALUE_STRING) {
                    size_t left_length = strlen(left.as.string != NULL ? left.as.string : "");
                    size_t right_length = strlen(right.as.string != NULL ? right.as.string : "");
                    char *joined;
                    if (left_length > SIZE_MAX - right_length - 1) {
                        value_free(&left);
                        value_free(&right);
                        memory_error(interpreter);
                        return false;
                    }
                    joined = (char *)malloc(left_length + right_length + 1);
                    if (joined == NULL) {
                        value_free(&left);
                        value_free(&right);
                        memory_error(interpreter);
                        return false;
                    }
                    memcpy(joined, left.as.string != NULL ? left.as.string : "", left_length);
                    memcpy(joined + left_length,
                           right.as.string != NULL ? right.as.string : "", right_length + 1);
                    result->type = VALUE_STRING;
                    result->as.string = joined;
                } else {
                    value_free(&left);
                    value_free(&right);
                    runtime_error(interpreter, "Operands must be two numbers or two strings.");
                    return false;
                }
            } else {
                if (left.type != VALUE_NUMBER || right.type != VALUE_NUMBER) {
                    value_free(&left);
                    value_free(&right);
                    runtime_error(interpreter, "Operands must be numbers.");
                    return false;
                }
                a = left.as.number;
                b = right.as.number;
                switch (op) {
                    case MINUS: *result = value_number(a - b); break;
                    case SLASH: *result = value_number(a / b); break;
                    case STAR: *result = value_number(a * b); break;
                    case GREATER: *result = value_bool(a > b); break;
                    case GREATER_EQUAL: *result = value_bool(a >= b); break;
                    case LESS: *result = value_bool(a < b); break;
                    case LESS_EQUAL: *result = value_bool(a <= b); break;
                    default:
                        value_free(&left);
                        value_free(&right);
                        runtime_error(interpreter, "Unsupported binary operator.");
                        return false;
                }
            }
            value_free(&left);
            value_free(&right);
            return true;
        }

        case EXPR_LOGICAL: {
            Value left = value_nil();
            TokenType op = expression->as.logical.op.type;
            if (!evaluate_expression(interpreter, expression->as.logical.left, &left)) {
                return false;
            }
            if (op == OR && value_is_truthy(left)) {
                *result = left;
                return true;
            }
            if (op == AND && !value_is_truthy(left)) {
                *result = left;
                return true;
            }
            value_free(&left);
            return evaluate_expression(interpreter, expression->as.logical.right, result);
        }

        case EXPR_CALL: {
            Value callee = value_nil();
            Value *arguments = NULL;
            size_t count = expression->as.call.arguments.count;
            size_t i;
            bool ok;
            if (!evaluate_expression(interpreter, expression->as.call.callee, &callee)) {
                return false;
            }
            if (count > 0) {
                if (count > SIZE_MAX / sizeof(*arguments)) {
                    value_free(&callee);
                    memory_error(interpreter);
                    return false;
                }
                arguments = (Value *)calloc(count, sizeof(*arguments));
                if (arguments == NULL) {
                    value_free(&callee);
                    memory_error(interpreter);
                    return false;
                }
                for (i = 0; i < count; ++i) {
                    arguments[i] = value_nil();
                    if (!evaluate_expression(interpreter,
                                             expression->as.call.arguments.items[i],
                                             &arguments[i])) {
                        size_t j;
                        for (j = 0; j <= i; ++j) value_free(&arguments[j]);
                        free(arguments);
                        value_free(&callee);
                        return false;
                    }
                }
            }
            ok = call_callable(interpreter, callee, arguments, count, result);
            for (i = 0; i < count; ++i) value_free(&arguments[i]);
            free(arguments);
            value_free(&callee);
            return ok;
        }

        case EXPR_GET: {
            Value object = value_nil();
            bool ok;
            if (!evaluate_expression(interpreter, expression->as.get.object, &object)) {
                return false;
            }
            if (object.type != VALUE_INSTANCE) {
                value_free(&object);
                runtime_error(interpreter, "Only instances have properties.");
                return false;
            }
            ok = get_instance_property(interpreter, object.as.object,
                                       &expression->as.get.name, result);
            value_free(&object);
            return ok;
        }

        case EXPR_SET: {
            Value object = value_nil();
            Value value = value_nil();
            bool ok;
            if (!evaluate_expression(interpreter, expression->as.set.object, &object)) {
                return false;
            }
            if (object.type != VALUE_INSTANCE) {
                value_free(&object);
                runtime_error(interpreter, "Only instances have fields.");
                return false;
            }
            if (!evaluate_expression(interpreter, expression->as.set.value, &value)) {
                value_free(&object);
                return false;
            }
            ok = set_instance_field(interpreter, object.as.object,
                                    expression->as.set.name.lexeme, &value);
            value_free(&object);
            if (!ok) {
                value_free(&value);
                return false;
            }
            *result = value;
            return true;
        }

        case EXPR_SUPER: {
            size_t distance;
            Value superclass_value = value_nil();
            Value this_value = value_nil();
            RuntimeObject *superclass;
            RuntimeObject *instance;
            RuntimeObject *method;
            RuntimeObject *bound;
            if (!interpreter_get_resolution(interpreter, expression, &distance)) {
                runtime_error(interpreter, "Unable to resolve 'super'.");
                return false;
            }
            if (distance == 0 || distance > (size_t)INT_MAX) {
                runtime_error(interpreter, "Invalid lexical distance for 'super'.");
                return false;
            }
            if (!read_value_at(interpreter, interpreter->environment,
                               (int)distance, "super", &superclass_value)) {
                return false;
            }
            if (superclass_value.type != VALUE_CLASS || superclass_value.as.object == NULL) {
                value_free(&superclass_value);
                runtime_error(interpreter, "Invalid superclass.");
                return false;
            }
            if (!read_value_at(interpreter, interpreter->environment,
                               (int)(distance - 1), "this", &this_value)) {
                value_free(&superclass_value);
                return false;
            }
            if (this_value.type != VALUE_INSTANCE || this_value.as.object == NULL) {
                value_free(&superclass_value);
                value_free(&this_value);
                runtime_error(interpreter, "Invalid 'this' reference.");
                return false;
            }
            superclass = superclass_value.as.object;
            instance = this_value.as.object;
            method = find_method(superclass, expression->as.super_expr.method.lexeme);
            if (method == NULL) {
                value_free(&superclass_value);
                value_free(&this_value);
                runtime_error(interpreter, "Undefined property '%s'.",
                              expression->as.super_expr.method.lexeme != NULL
                                  ? expression->as.super_expr.method.lexeme : "");
                return false;
            }
            bound = bind_method(interpreter, method, instance);
            value_free(&superclass_value);
            value_free(&this_value);
            if (bound == NULL) {
                return false;
            }
            result->type = VALUE_FUNCTION;
            result->as.object = bound;
            return true;
        }
    }

    runtime_error(interpreter, "Unsupported expression node.");
    return false;
}

static bool execute_statement_list(
    Interpreter *interpreter,
    Stmt *const *statements,
    size_t count,
    bool stop_on_error
)
{
    size_t i;
    bool success = true;
    if (count > 0 && statements == NULL) {
        runtime_error(interpreter, "Invalid statement list.");
        return false;
    }
    for (i = 0; i < count; ++i) {
        bool statement_ok;
        if (interpreter->returning) {
            break;
        }
        statement_ok = execute_statement(interpreter, statements[i]);
        if (!statement_ok) {
            success = false;
            if (stop_on_error) {
                break;
            }
        }
    }
    return success;
}

static bool execute_statement(Interpreter *interpreter, Stmt *statement)
{
    if (interpreter == NULL || statement == NULL) {
        return statement == NULL;
    }
    if (interpreter->returning) {
        return true;
    }

    switch (statement->type) {
        case STMT_EXPRESSION: {
            Value value = value_nil();
            bool ok = evaluate_expression(interpreter,
                                          statement->as.expression.expression, &value);
            value_free(&value);
            return ok;
        }

        case STMT_PRINT: {
            Value value = value_nil();
            char *text;
            bool ok = evaluate_expression(interpreter, statement->as.print.expression, &value);
            if (!ok) {
                value_free(&value);
                return false;
            }
            text = value_to_string(value);
            if (text == NULL) {
                value_free(&value);
                memory_error(interpreter);
                return false;
            }
            fprintf(interpreter->out != NULL ? interpreter->out : stdout, "%s\n", text);
            free(text);
            value_free(&value);
            return true;
        }

        case STMT_VAR: {
            Value value = value_nil();
            bool ok = true;
            if (statement->as.var.initializer != NULL) {
                ok = evaluate_expression(interpreter, statement->as.var.initializer, &value);
            }
            if (ok) {
                ok = define_value_in_environment(interpreter, interpreter->environment,
                                                 statement->as.var.name.lexeme, &value);
            }
            value_free(&value);
            return ok;
        }

        case STMT_BLOCK: {
            Environment *block_environment = new_environment(interpreter, interpreter->environment);
            Environment *previous;
            bool ok;
            if (block_environment == NULL) return false;
            previous = interpreter->environment;
            interpreter->environment = block_environment;
            ok = execute_statement_list(interpreter,
                                        statement->as.block.statements.items,
                                        statement->as.block.statements.count,
                                        true);
            interpreter->environment = previous;
            return ok;
        }

        case STMT_IF: {
            Value condition = value_nil();
            bool truthy;
            if (!evaluate_expression(interpreter, statement->as.if_stmt.condition, &condition)) {
                value_free(&condition);
                return false;
            }
            truthy = value_is_truthy(condition);
            value_free(&condition);
            if (truthy) {
                return statement->as.if_stmt.then_branch == NULL
                    ? true : execute_statement(interpreter, statement->as.if_stmt.then_branch);
            }
            return statement->as.if_stmt.else_branch == NULL
                ? true : execute_statement(interpreter, statement->as.if_stmt.else_branch);
        }

        case STMT_WHILE: {
            while (!interpreter->returning) {
                Value condition = value_nil();
                bool truthy;
                if (!evaluate_expression(interpreter, statement->as.while_stmt.condition,
                                         &condition)) {
                    value_free(&condition);
                    return false;
                }
                truthy = value_is_truthy(condition);
                value_free(&condition);
                if (!truthy) break;
                if (!execute_statement(interpreter, statement->as.while_stmt.body)) {
                    return false;
                }
            }
            return true;
        }

        case STMT_FUNCTION: {
            RuntimeObject *function = new_function(interpreter, statement,
                                                   interpreter->environment, false);
            Value value = value_nil();
            bool ok;
            if (function == NULL) return false;
            value.type = VALUE_FUNCTION;
            value.as.object = function;
            ok = define_value_in_environment(interpreter, interpreter->environment,
                                             statement->as.function.name.lexeme, &value);
            return ok;
        }

        case STMT_RETURN: {
            Value value = value_nil();
            if (statement->as.return_stmt.value != NULL &&
                !evaluate_expression(interpreter, statement->as.return_stmt.value, &value)) {
                value_free(&value);
                return false;
            }
            value_free(&interpreter->return_value);
            interpreter->return_value = value;
            interpreter->returning = true;
            return true;
        }

        case STMT_CLASS: {
            RuntimeObject *superclass = NULL;
            RuntimeObject *klass;
            Environment *previous_environment = interpreter->environment;
            bool has_superclass = statement->as.class_stmt.superclass != NULL;
            Value superclass_value = value_nil();
            Value placeholder = value_nil();
            Value class_value = value_nil();
            size_t i;
            bool ok = true;

            /* Match C++ order: evaluate and validate superclass before defining class name. */
            if (has_superclass) {
                if (!evaluate_expression(interpreter, statement->as.class_stmt.superclass,
                                         &superclass_value)) {
                    return false;
                }
                if (superclass_value.type != VALUE_CLASS || superclass_value.as.object == NULL) {
                    value_free(&superclass_value);
                    runtime_error(interpreter, "Superclass must be a class.");
                    return false;
                }
                superclass = superclass_value.as.object;
            }

            if (!define_value_in_environment(interpreter, interpreter->environment,
                                             statement->as.class_stmt.name.lexeme,
                                             &placeholder)) {
                value_free(&superclass_value);
                return false;
            }

            if (has_superclass) {
                Environment *super_environment = new_environment(interpreter, interpreter->environment);
                Value super_value = value_nil();
                if (super_environment == NULL) {
                    value_free(&superclass_value);
                    return false;
                }
                interpreter->environment = super_environment;
                super_value.type = VALUE_CLASS;
                super_value.as.object = superclass;
                if (!define_value_in_environment(interpreter, interpreter->environment,
                                                 "super", &super_value)) {
                    interpreter->environment = previous_environment;
                    value_free(&superclass_value);
                    return false;
                }
            }

            klass = new_object(interpreter, OBJECT_CLASS);
            if (klass == NULL) {
                interpreter->environment = previous_environment;
                value_free(&superclass_value);
                return false;
            }
            klass->as.klass.name = copy_text(statement->as.class_stmt.name.lexeme);
            klass->as.klass.superclass = superclass;
            if (klass->as.klass.name == NULL) {
                memory_error(interpreter);
                interpreter->environment = previous_environment;
                value_free(&superclass_value);
                return false;
            }

            for (i = 0; i < statement->as.class_stmt.methods.count; ++i) {
                Stmt *method_statement = statement->as.class_stmt.methods.items[i];
                RuntimeObject *method_function;
                MethodEntry *entry;
                const char *method_name;
                bool initializer;
                if (method_statement == NULL || method_statement->type != STMT_FUNCTION) {
                    runtime_error(interpreter, "Invalid method declaration in class.");
                    ok = false;
                    break;
                }
                method_name = method_statement->as.function.name.lexeme != NULL
                    ? method_statement->as.function.name.lexeme : "";
                initializer = strcmp(method_name, "init") == 0;
                method_function = new_function(interpreter, method_statement,
                                               interpreter->environment, initializer);
                if (method_function == NULL) {
                    ok = false;
                    break;
                }
                entry = (MethodEntry *)calloc(1, sizeof(*entry));
                if (entry == NULL) {
                    memory_error(interpreter);
                    ok = false;
                    break;
                }
                entry->name = copy_text(method_name);
                if (entry->name == NULL) {
                    free(entry);
                    memory_error(interpreter);
                    ok = false;
                    break;
                }
                entry->function = method_function;
                entry->next = klass->as.klass.methods;
                klass->as.klass.methods = entry;
            }

            interpreter->environment = previous_environment;
            value_free(&superclass_value);
            if (!ok) return false;

            class_value.type = VALUE_CLASS;
            class_value.as.object = klass;
            /* The class-name placeholder is in the original scope. */
            if (!assign_value_at(interpreter, previous_environment, 0,
                                 statement->as.class_stmt.name.lexeme, &class_value)) {
                return false;
            }
            return true;
        }
    }

    runtime_error(interpreter, "Unsupported statement node.");
    return false;
}

bool interpreter_evaluate_value(Interpreter *interpreter, Expr *expression, Value *result)
{
    return evaluate_expression(interpreter, expression, result);
}

bool interpreter_evaluate(Interpreter *interpreter, Expr *expression)
{
    Value result = value_nil();
    bool ok = evaluate_expression(interpreter, expression, &result);
    value_free(&result);
    return ok;
}

bool interpreter_interpret(
    Interpreter *interpreter,
    Stmt *const *statements,
    size_t statement_count
)
{
    bool success = true;
    size_t i;
    if (interpreter == NULL || interpreter->environment == NULL ||
        (statement_count > 0 && statements == NULL)) {
        return false;
    }
    for (i = 0; i < statement_count; ++i) {
        if (!execute_statement(interpreter, statements[i])) {
            success = false;
        }
        /* Top-level continues after one runtime error, matching the existing C driver. */
    }
    return success && !interpreter->had_error;
}

bool interpreter_interpret_list(Interpreter *interpreter, const StmtList *statements)
{
    if (statements == NULL) {
        return false;
    }
    return interpreter_interpret(interpreter, statements->items, statements->count);
}

static void free_runtime_objects(RuntimeObject *object)
{
    while (object != NULL) {
        RuntimeObject *next = object->next;
        if (object->type == OBJECT_CLASS) {
            MethodEntry *method = object->as.klass.methods;
            free(object->as.klass.name);
            while (method != NULL) {
                MethodEntry *method_next = method->next;
                free(method->name);
                free(method);
                method = method_next;
            }
        } else if (object->type == OBJECT_INSTANCE) {
            FieldEntry *field = object->as.instance.fields;
            while (field != NULL) {
                FieldEntry *field_next = field->next;
                free(field->name);
                value_free(&field->value);
                free(field);
                field = field_next;
            }
        }
        free(object);
        object = next;
    }
}

void interpreter_free(Interpreter *interpreter)
{
    InterpreterEnvironmentNode *node;
    if (interpreter == NULL) {
        return;
    }

    /* Environment values are separately allocated Value objects, not owned by
     * environment.c. Free them before releasing any environment storage. */
    for (node = interpreter->environments; node != NULL; node = node->next) {
        EnvironmentEntry *entry;
        for (entry = node->environment->entries; entry != NULL; entry = entry->next) {
            Value *value = (Value *)entry->value;
            if (value != NULL) {
                value_free(value);
                free(value);
                entry->value = NULL;
            }
        }
    }

    value_free(&interpreter->return_value);
    free_runtime_objects(interpreter->objects);
    interpreter->objects = NULL;
    free(interpreter->locals);
    interpreter->locals = NULL;
    interpreter->local_count = 0;
    interpreter->local_capacity = 0;

    /* The registry is newest-first. Releasing in that order releases child
     * environments before parents, leaving each parent's registry reference
     * intact until its turn. */
    node = interpreter->environments;
    while (node != NULL) {
        InterpreterEnvironmentNode *next = node->next;
        environment_free(node->environment);
        free(node);
        node = next;
    }
    interpreter->environments = NULL;
    interpreter->globals = NULL;
    interpreter->environment = NULL;
    interpreter->returning = false;
    interpreter->had_error = false;
    interpreter->out_of_memory = false;
}
