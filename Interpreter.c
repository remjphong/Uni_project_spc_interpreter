#include "interpreter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static char *copy_text(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

Value value_nil(void) {
    Value v; v.type = VALUE_NIL; return v;
}
Value value_bool(bool b) {
    Value v; v.type = VALUE_BOOL; v.as.boolean = b; return v;
}
Value value_number(double n) {
    Value v; v.type = VALUE_NUMBER; v.as.number = n; return v;
}
Value value_string(const char *s) {
    Value v; v.type = VALUE_STRING; v.as.string = copy_text(s); return v;
}
void value_free(Value *v) {
    if (v && v->type == VALUE_STRING) free(v->as.string);
    if (v) *v = value_nil();
}
bool value_is_truthy(Value v) {
    if (v.type == VALUE_NIL) return false;
    if (v.type == VALUE_BOOL) return v.as.boolean;
    return true;
}
bool value_is_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VALUE_NIL: return true;
        case VALUE_BOOL: return a.as.boolean == b.as.boolean;
        case VALUE_NUMBER: return a.as.number == b.as.number;
        case VALUE_STRING:
            return strcmp(a.as.string ? a.as.string : "",
                          b.as.string ? b.as.string : "") == 0;
    }
    return false;
}
char *value_to_string(Value v) {
    char buffer[128];
    switch (v.type) {
        case VALUE_NIL: return copy_text("nil");
        case VALUE_BOOL: return copy_text(v.as.boolean ? "true" : "false");
        case VALUE_NUMBER:
            snprintf(buffer, sizeof(buffer), "%.15g", v.as.number);
            return copy_text(buffer);
        case VALUE_STRING: return copy_text(v.as.string ? v.as.string : "");
    }
    return copy_text("nil");
}

static void runtime_error(Interpreter *in, const char *fmt, ...) {
    if (!in) return;
    in->had_error = true;
    FILE *err = in->err ? in->err : stderr;
    va_list args;
    va_start(args, fmt);
    fputs("Runtime error: ", err);
    vfprintf(err, fmt, args);
    fputc('\n', err);
    va_end(args);
}

static Value *new_value(Value v) {
    Value *p = (Value *)malloc(sizeof(*p));
    if (p) *p = v;
    return p;
}

static bool define_value(Interpreter *in, const char *name, Value v) {
    Value *p = new_value(v);
    if (!p) {
        runtime_error(in, "Không đủ bộ nhớ.");
        return false;
    }
    if (!environment_define(in->environment, name, p)) {
        value_free(p);
        free(p);
        runtime_error(in, "Không thể tạo biến '%s'.", name ? name : "");
        return false;
    }
    return true;
}

static bool assign_value(Interpreter *in, const char *name, Value v) {
    
    Value *old = (Value *)environment_get(in->environment, name);
    if (!old) {
        runtime_error(in, "Biến '%s' chưa được khai báo.", name ? name : "");
        return false;
    }
    Value *p = new_value(v);
    if (!p) {
        runtime_error(in, "Không đủ bộ nhớ.");
        return false;
    }
    if (!environment_assign(in->environment, name, p)) {
        value_free(p); free(p);
        runtime_error(in, "Biến '%s' chưa được khai báo.", name ? name : "");
        return false;
    }
    value_free(old);
    free(old);
    return true;
}

void interpreter_init(Interpreter *in, FILE *out, FILE *err) {
    if (!in) return;
    in->globals = environment_new(NULL);
    in->environment = in->globals;
    in->out = out ? out : stdout;
    in->err = err ? err : stderr;
    in->had_error = (in->globals == NULL);
}

void interpreter_free(Interpreter *in) {
    if (!in) return;
    
    Environment *env = in->globals;
    while (env) {
        for (EnvironmentEntry *e = env->entries; e; e = e->next) {
            Value *v = (Value *)e->value;
            if (v) { value_free(v); free(v); e->value = NULL; }
        }
        Environment *next = env->enclosing;
        environment_free(env);
        env = next;
    }
    in->globals = NULL;
    in->environment = NULL;
}

static bool evaluate_expr(Interpreter *in, Expr *expr, Value *result) {
    if (!expr || !result) {
        runtime_error(in, "Biểu thức không hợp lệ.");
        return false;
    }

    switch (expr->type) {
        case EXPR_LITERAL:
            switch (expr->as.literal.value.type) {
                case LIT_NIL: *result = value_nil(); return true;
                case LIT_BOOL: *result = value_bool(expr->as.literal.value.as.boolean); return true;
                case LIT_NUMBER: *result = value_number(expr->as.literal.value.as.number); return true;
                case LIT_STRING: *result = value_string(expr->as.literal.value.as.string); return true;
            }
            break;

        case EXPR_GROUPING:
            return evaluate_expr(in, expr->as.grouping.expression, result);

        case EXPR_VARIABLE: {
            const char *name = expr->as.variable.name.lexeme;
            Value *stored = (Value *)environment_get(in->environment, name);
            if (!stored) {
                runtime_error(in, "Biến '%s' chưa được khai báo.", name ? name : "");
                return false;
            }
            *result = *stored;
            if (stored->type == VALUE_STRING)
                result->as.string = copy_text(stored->as.string);
            return true;
        }

        case EXPR_UNARY: {
            Value right;
            if (!evaluate_expr(in, expr->as.unary.right, &right)) return false;
            TokenType op = expr->as.unary.op.type;
            if (op == TOKEN_MINUS) {
                if (right.type != VALUE_NUMBER) {
                    runtime_error(in, "Toán tử '-' cần một số.");
                    value_free(&right); return false;
                }
                *result = value_number(-right.as.number);
                value_free(&right); return true;
            }
            if (op == TOKEN_BANG) {
                *result = value_bool(!value_is_truthy(right));
                value_free(&right); return true;
            }
            value_free(&right);
            runtime_error(in, "Toán tử một ngôi không được hỗ trợ.");
            return false;
        }

        case EXPR_BINARY: {
            Value left, right;
            if (!evaluate_expr(in, expr->as.binary.left, &left)) return false;
            if (!evaluate_expr(in, expr->as.binary.right, &right)) {
                value_free(&left); return false;
            }
            TokenType op = expr->as.binary.op.type;
            if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL) {
                bool equal = value_is_equal(left, right);
                *result = value_bool(op == TOKEN_EQUAL_EQUAL ? equal : !equal);
            } else if (op == TOKEN_PLUS) {
                if (left.type == VALUE_NUMBER && right.type == VALUE_NUMBER) {
                    *result = value_number(left.as.number + right.as.number);
                } else if (left.type == VALUE_STRING && right.type == VALUE_STRING) {
                    size_t a = strlen(left.as.string ? left.as.string : "");
                    size_t b = strlen(right.as.string ? right.as.string : "");
                    char *joined = (char *)malloc(a + b + 1);
                    if (!joined) {
                        runtime_error(in, "Không đủ bộ nhớ.");
                        value_free(&left); value_free(&right); return false;
                    }
                    memcpy(joined, left.as.string ? left.as.string : "", a);
                    memcpy(joined + a, right.as.string ? right.as.string : "", b + 1);
                    *result = value_string(joined);
                    free(joined);
                } else {
                    runtime_error(in, "Toán tử '+' cần hai số hoặc hai chuỗi.");
                    value_free(&left); value_free(&right); return false;
                }
            } else if (op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH ||
                       op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL ||
                       op == TOKEN_LESS || op == TOKEN_LESS_EQUAL) {
                if (left.type != VALUE_NUMBER || right.type != VALUE_NUMBER) {
                    runtime_error(in, "Toán tử số học/so sánh cần hai số.");
                    value_free(&left); value_free(&right); return false;
                }
                double a = left.as.number, b = right.as.number;
                switch (op) {
                    case TOKEN_MINUS: *result = value_number(a - b); break;
                    case TOKEN_STAR: *result = value_number(a * b); break;
                    case TOKEN_SLASH:
                        if (b == 0.0) {
                            runtime_error(in, "Không thể chia cho 0.");
                            value_free(&left); value_free(&right); return false;
                        }
                        *result = value_number(a / b); break;
                    case TOKEN_GREATER: *result = value_bool(a > b); break;
                    case TOKEN_GREATER_EQUAL: *result = value_bool(a >= b); break;
                    case TOKEN_LESS: *result = value_bool(a < b); break;
                    case TOKEN_LESS_EQUAL: *result = value_bool(a <= b); break;
                    default: break;
                }
            } else {
                runtime_error(in, "Toán tử nhị phân không được hỗ trợ.");
                value_free(&left); value_free(&right); return false;
            }
            value_free(&left);
            value_free(&right);
            return true;
        }

        default:
            runtime_error(in, "Loại biểu thức này chưa được triển khai.");
            return false;
    }
    runtime_error(in, "Literal không hợp lệ.");
    return false;
}

static bool execute_stmt(Interpreter *in, Stmt *stmt) {
    if (!stmt) return true;
    switch (stmt->type) {
        case STMT_EXPRESSION: {
            Value v;
            if (!evaluate_expr(in, stmt->as.expression.expression, &v)) return false;
            value_free(&v);
            return true;
        }
        case STMT_PRINT: {
            Value v;
            if (!evaluate_expr(in, stmt->as.print.expression, &v)) return false;
            char *s = value_to_string(v);
            fprintf(in->out ? in->out : stdout, "%s\n", s ? s : "");
            free(s);
            value_free(&v);
            return true;
        }
        case STMT_VAR: {
            Value v = value_nil();
            if (stmt->as.var.initializer &&
                !evaluate_expr(in, stmt->as.var.initializer, &v)) return false;
            bool ok = define_value(in, stmt->as.var.name.lexeme, v);
            if (!ok) value_free(&v);
            return ok;
        }
        default:
            runtime_error(in, "Loại câu lệnh này chưa được triển khai.");
            return false;
    }
}

bool interpreter_evaluate(Interpreter *in, Expr *expression) {
    Value v;
    if (!evaluate_expr(in, expression, &v)) return false;
    value_free(&v);
    return true;
}

bool interpreter_interpret(Interpreter *in, Stmt *statements) {
    if (!in || !in->environment) return false;
    bool ok = true;
    for (Stmt *stmt = statements; stmt; stmt = stmt->next) {
        if (!execute_stmt(in, stmt)) ok = false;
    }
    return ok && !in->had_error;
}
