#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "AST.h"

/* Parser uses a borrowed view of the tokens; AST nodes copy stored lexemes. */
typedef struct Parser {
    const Token *tokens;
    size_t token_count;
    size_t current;
    bool panic_mode;
    bool had_error;
    bool out_of_memory;
} Parser;

void parser_init(Parser *parser, const Token *tokens, size_t token_count);

/*
 * Parse the token stream into out_statements.
 * out_statements must be empty/zero-initialized before calling.
 * Returns true only if parsing completed without syntax or memory errors.
 * After recoverable syntax errors, valid statements may still be present.
 * The caller must call stmt_list_free() when finished with the AST.
 */
bool parser_parse(Parser *parser, StmtList *out_statements);

#endif /* PARSER_H */
