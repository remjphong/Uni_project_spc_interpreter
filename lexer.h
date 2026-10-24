#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stddef.h>

/*
 * C equivalent of the original C++ Lexer interface.
 *
 * The old C++ interface exposed:
 *   Lexer(std::string src);
 *   std::vector<Token> scanTokens();
 *
 * In C, the source and scanner state are initialized explicitly, and the
 * returned token sequence is represented by TokenArray. The tokenization
 * rules and token positions are intended to match the original C++ lexer.
 */

typedef struct Lexer {
    const char *source; /* Borrowed, null-terminated source; not owned by Lexer. */
    size_t cursor;      /* Current byte offset in source. */
    int line;           /* 1-based current line. */
    int column;         /* 1-based current column. */
} Lexer;

typedef struct TokenArray {
    Token *items;       /* Dynamically allocated tokens. */
    size_t count;       /* Number of valid tokens. */
    size_t capacity;    /* Allocated number of token slots. */
} TokenArray;

/*
 * Initialize an empty token array before passing it to scanTokens() or
 * scanFile(). Safe to call with NULL (it does nothing).
 */
void initTokenArray(TokenArray *tokens);

/*
 * Initialize the lexer state, equivalent to the original C++ constructor.
 * The source must be non-NULL, null-terminated, and remain valid throughout
 * scanning. This function does not copy or own the source buffer.
 */
void initLexer(Lexer *lexer, const char *source);

/*
 * Tokenize the source and append TOKEN_EOF.
 *
 * Preconditions:
 *   - lexer has been initialized by initLexer();
 *   - tokens has been initialized by initTokenArray() and is empty.
 *
 * Returns 1 when scanning completes. Lexical errors are represented as
 * TOKEN_ERROR tokens and do not, by themselves, make this function fail.
 * Returns 0 for invalid arguments or an internal failure (for example,
 * memory allocation failure). On internal scanning failure, any partial
 * tokens are freed and the array is reset to the empty state.
 */
int scanTokens(Lexer *lexer, TokenArray *tokens);

/*
 * Read a source file, tokenize it, and append TOKEN_EOF.
 * tokens must first be initialized with initTokenArray() and be empty.
 * Returns 1 on successful file reading and scanning, or 0 on I/O/internal
 * failure. Lexical errors are still returned in TOKEN_ERROR tokens.
 */
int scanFile(const char *filename, TokenArray *tokens);

/*
 * Read an entire file into a dynamically allocated, null-terminated buffer.
 * On success, stores the buffer in *source; the caller owns it and must call
 * free(*source). Embedded NUL bytes are rejected by the implementation.
 * Returns 1 on success, 0 on failure. On failure, *source is set to NULL.
 */
int readSourceFile(const char *filename, char **source);

/*
 * Free each token lexeme and the token storage, then reset the array.
 * The array should have been initialized by initTokenArray().
 */
void freeTokens(TokenArray *tokens);

#endif /* LEXER_H */
