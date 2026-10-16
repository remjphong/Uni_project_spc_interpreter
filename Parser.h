#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include "AST.h"

typedef struct {
    Stmt** items; // pointer to pointers 
    size_t count;
    size_t capacity;
} StmtList;

typedef struct {
    const Token* tokens;
    size_t token_count;
    size_t current;
    bool had_error;   
    bool panic_mode;  
} Parser;

void parser_init(Parser* parser, const Token* tokens, size_t token_count);
StmtList parser_parse(Parser* parser);
void stmt_list_free(StmtList* list);

#endif