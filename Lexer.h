#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stddef.h>

/* Lexer state */
typedef struct {
    const char *source;
    size_t cursor;
    int line;
    int column;
} Lexer;

/* Dynamic array of tokens */
typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenArray;

/*
 * Initialize a token array before using it.
 */
void initTokenArray(TokenArray *tokens);

/*
 * Initialize the lexer with a null-terminated source string.
 * The source must remain valid until scanning finishes.
 */
void initLexer(Lexer *lexer, const char *source);

/*
 * Scan source code and append an EOF token.
 *
 * Returns:
 *   1 = scanning completed (TOKEN_ERROR may still exist)
 *   0 = internal failure, such as memory allocation failure
 *
 * Call initTokenArray() before calling this function.
 */
int scanTokens(Lexer *lexer, TokenArray *tokens);

/*
 * Read a real source file and tokenize its contents.
 *
 * Returns:
 *   1 = file read and scanning completed
 *   0 = file I/O or internal failure
 *
 * Lexical errors are represented by TOKEN_ERROR tokens.
 */
int scanFile(const char *filename, TokenArray *tokens);

/*
 * Read the entire file into a dynamically allocated,
 * null-terminated buffer. Caller must free(*source).
 *
 * Returns 1 on success, 0 on failure.
 */
int readSourceFile(const char *filename, char **source);

/* Free every token lexeme and the token array */
void freeTokens(TokenArray *tokens);

#endif /* LEXER_H */
