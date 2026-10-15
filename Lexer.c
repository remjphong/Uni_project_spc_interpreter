#include "lexer.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------
 * Token array and memory management
 * --------------------------------------------------------- */

void initTokenArray(TokenArray *tokens)
{
    if (tokens == NULL) {
        return;
    }

    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}

void freeTokens(TokenArray *tokens)
{
    size_t i;

    if (tokens == NULL) {
        return;
    }

    for (i = 0; i < tokens->count; i++) {
        free(tokens->items[i].lexeme);
    }

    free(tokens->items);
    initTokenArray(tokens);
}

static char *copyString(const char *source, size_t length)
{
    char *copy;

    if (source == NULL || length == SIZE_MAX) {
        return NULL;
    }

    copy = (char *)malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, source, length);
    copy[length] = '\0';

    return copy;
}

static int addToken(
    TokenArray *tokens,
    TokenType type,
    const char *lexeme,
    size_t length,
    int line,
    int column
)
{
    Token *newItems;
    size_t newCapacity;
    char *lexemeCopy;

    if (tokens == NULL || lexeme == NULL) {
        return 0;
    }

    lexemeCopy = copyString(lexeme, length);

    if (lexemeCopy == NULL) {
        return 0;
    }

    if (tokens->count == tokens->capacity) {
        if (tokens->capacity == 0) {
            newCapacity = 16;
        } else {
            if (tokens->capacity > SIZE_MAX / 2) {
                free(lexemeCopy);
                return 0;
            }

            newCapacity = tokens->capacity * 2;
        }

        if (newCapacity > SIZE_MAX / sizeof(Token)) {
            free(lexemeCopy);
            return 0;
        }

        newItems = (Token *)realloc(
            tokens->items,
            newCapacity * sizeof(Token)
        );

        if (newItems == NULL) {
            free(lexemeCopy);
            return 0;
        }

        tokens->items = newItems;
        tokens->capacity = newCapacity;
    }

    tokens->items[tokens->count].type = type;
    tokens->items[tokens->count].lexeme = lexemeCopy;
    tokens->items[tokens->count].line = line;
    tokens->items[tokens->count].column = column;

    tokens->count++;

    return 1;
}

/* ---------------------------------------------------------
 * Lexer initialization and character operations
 * --------------------------------------------------------- */

void initLexer(Lexer *lexer, const char *source)
{
    if (lexer == NULL) {
        return;
    }

    lexer->source = source;
    lexer->cursor = 0;
    lexer->line = 1;
    lexer->column = 1;
}

static int isAtEnd(const Lexer *lexer)
{
    return lexer->source[lexer->cursor] == '\0';
}

static char peek(const Lexer *lexer)
{
    return lexer->source[lexer->cursor];
}

static char peekNext(const Lexer *lexer)
{
    if (isAtEnd(lexer) || lexer->source[lexer->cursor + 1] == '\0') {
        return '\0';
    }

    return lexer->source[lexer->cursor + 1];
}

/*
 * Consume one character.
 * Line and column are updated immediately.
 */
static char advance(Lexer *lexer)
{
    char c = lexer->source[lexer->cursor];

    if (c == '\0') {
        return '\0';
    }

    lexer->cursor++;

    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }

    return c;
}

static int match(Lexer *lexer, char expected)
{
    if (isAtEnd(lexer) || peek(lexer) != expected) {
        return 0;
    }

    advance(lexer);
    return 1;
}

/* ---------------------------------------------------------
 * Character classification
 * --------------------------------------------------------- */

static int isDigit(char c)
{
    return isdigit((unsigned char)c);
}

static int isAlpha(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int isAlphaNumeric(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* ---------------------------------------------------------
 * Keywords
 * --------------------------------------------------------- */

typedef struct {
    const char *name;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"and",    AND},
    {"class",  CLASS},
    {"else",   ELSE},
    {"false",  FALSE},
    {"for",    FOR},
    {"fun",    FUN},
    {"if",     IF},
    {"nil",    NIL},
    {"or",     OR},
    {"print",  PRINT},
    {"return", RETURN},
    {"super",  SUPER},
    {"this",   THIS},
    {"true",   TRUE},
    {"var",    VAR},
    {"while",  WHILE}
};

static TokenType identifierType(const char *text, size_t length)
{
    size_t i;

    for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        size_t keywordLength = strlen(keywords[i].name);

        if (length == keywordLength
            && memcmp(text, keywords[i].name, length) == 0) {
            return keywords[i].type;
        }
    }

    return IDENTIFIER;
}

/* ---------------------------------------------------------
 * Error-token helper
 * --------------------------------------------------------- */

static int addError(
    TokenArray *tokens,
    const char *message,
    int line,
    int column
)
{
    return addToken(
        tokens,
        TOKEN_ERROR,
        message,
        strlen(message),
        line,
        column
    );
}

/* ---------------------------------------------------------
 * Token scanning
 * --------------------------------------------------------- */

static int scanToken(Lexer *lexer, TokenArray *tokens)
{
    size_t start;
    size_t length;
    int startLine;
    int startColumn;
    char c;
    TokenType type;

    start = lexer->cursor;
    startLine = lexer->line;
    startColumn = lexer->column;

    c = advance(lexer);

    switch (c) {
        /* Single-character tokens */
        case '(':
            return addToken(tokens, LEFT_PAREN, "(", 1,
                            startLine, startColumn);

        case ')':
            return addToken(tokens, RIGHT_PAREN, ")", 1,
                            startLine, startColumn);

        case '{':
            return addToken(tokens, LEFT_BRACE, "{", 1,
                            startLine, startColumn);

        case '}':
            return addToken(tokens, RIGHT_BRACE, "}", 1,
                            startLine, startColumn);

        case ',':
            return addToken(tokens, COMMA, ",", 1,
                            startLine, startColumn);

        case '.':
            return addToken(tokens, DOT, ".", 1,
                            startLine, startColumn);

        case '-':
            return addToken(tokens, MINUS, "-", 1,
                            startLine, startColumn);

        case '+':
            return addToken(tokens, PLUS, "+", 1,
                            startLine, startColumn);

        case ';':
            return addToken(tokens, SEMICOLON, ";", 1,
                            startLine, startColumn);

        case '*':
            return addToken(tokens, STAR, "*", 1,
                            startLine, startColumn);

        /* One- or two-character operators */
        case '!':
            if (match(lexer, '=')) {
                return addToken(tokens, BANG_EQUAL, "!=" , 2,
                                startLine, startColumn);
            }

            return addToken(tokens, BANG, "!", 1,
                            startLine, startColumn);

        case '=':
            if (match(lexer, '=')) {
                return addToken(tokens, EQUAL_EQUAL, "==", 2,
                                startLine, startColumn);
            }

            return addToken(tokens, EQUAL, "=", 1,
                            startLine, startColumn);

        case '<':
            if (match(lexer, '=')) {
                return addToken(tokens, LESS_EQUAL, "<=", 2,
                                startLine, startColumn);
            }

            return addToken(tokens, LESS, "<", 1,
                            startLine, startColumn);

        case '>':
            if (match(lexer, '=')) {
                return addToken(tokens, GREATER_EQUAL, ">=", 2,
                                startLine, startColumn);
            }

            return addToken(tokens, GREATER, ">", 1,
                            startLine, startColumn);

        case '/':
            /* Single-line comment: // ... */
            if (match(lexer, '/')) {
                while (!isAtEnd(lexer) && peek(lexer) != '\n') {
                    advance(lexer);
                }

                /* The newline is handled by the next scan step. */
                return 1;
            }

            /* Block comment: /* ... * / */
            if (match(lexer, '*')) {
                int closed = 0;

                while (!isAtEnd(lexer)) {
                    if (peek(lexer) == '*'
                        && peekNext(lexer) == '/') {
                        advance(lexer);
                        advance(lexer);
                        closed = 1;
                        break;
                    }

                    advance(lexer);
                }

                if (!closed) {
                    return addError(
                        tokens,
                        "Unterminated block comment",
                        startLine,
                        startColumn
                    );
                }

                return 1;
            }

            return addToken(tokens, SLASH, "/", 1,
                            startLine, startColumn);

        /* Whitespace */
        case ' ':
        case '\r':
        case '\t':
        case '\n':
            return 1;

        /* String literal */
        case '"':
            while (!isAtEnd(lexer) && peek(lexer) != '"') {
                advance(lexer);
            }

            if (isAtEnd(lexer)) {
                return addError(
                    tokens,
                    "Unterminated string",
                    startLine,
                    startColumn
                );
            }

            /* Consume closing quote */
            advance(lexer);

            /*
             * Preserve the original convention:
             * the STRING lexeme excludes both quotation marks.
             */
            length = lexer->cursor - start - 2;

            return addToken(
                tokens,
                STRING,
                lexer->source + start + 1,
                length,
                startLine,
                startColumn
            );

        default:
            break;
    }

    /* Number: integer or decimal */
    if (isDigit(c)) {
        while (isDigit(peek(lexer))) {
            advance(lexer);
        }

        /*
         * A period belongs to the number only when
         * the next character is a digit.
         */
        if (peek(lexer) == '.' && isDigit(peekNext(lexer))) {
            advance(lexer);

            while (isDigit(peek(lexer))) {
                advance(lexer);
            }
        }

        length = lexer->cursor - start;

        return addToken(
            tokens,
            NUMBER,
            lexer->source + start,
            length,
            startLine,
            startColumn
        );
    }

    /* Identifier or keyword */
    if (isAlpha(c)) {
        while (isAlphaNumeric(peek(lexer))) {
            advance(lexer);
        }

        length = lexer->cursor - start;

        type = identifierType(
            lexer->source + start,
            length
        );

        return addToken(
            tokens,
            type,
            lexer->source + start,
            length,
            startLine,
            startColumn
        );
    }

/* Invalid character: preserve the original error format */
{
    static const char prefix[] = "Unexpected character: ";
    char message[sizeof(prefix) + 1];

    memcpy(message, prefix, sizeof(prefix) - 1);
    message[sizeof(prefix) - 1] = c;
    message[sizeof(prefix)] = '\0';

    return addError(
        tokens,
        message,
        startLine,
        startColumn
    );
}
}
/* ---------------------------------------------------------
 * Scan a source string
 * --------------------------------------------------------- */

int scanTokens(Lexer *lexer, TokenArray *tokens)
{
    if (lexer == NULL || tokens == NULL || lexer->source == NULL) {
        return 0;
    }

    /*
     * Contract: tokens must have been initialized with
     * initTokenArray() and must be empty before scanning.
     */
    if (tokens->count != 0 || tokens->items != NULL
        || tokens->capacity != 0) {
        return 0;
    }

    while (!isAtEnd(lexer)) {
        if (!scanToken(lexer, tokens)) {
            freeTokens(tokens);
            return 0;
        }
    }

    /* EOF lexeme is an empty string */
    if (!addToken(
            tokens,
            TOKEN_EOF,
            "",
            0,
            lexer->line,
            lexer->column)) {
        freeTokens(tokens);
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------
 * Read source from a real file
 * --------------------------------------------------------- */

int readSourceFile(const char *filename, char **source)
{
    FILE *file;
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 4096;
    size_t bytesRead;

    if (filename == NULL || source == NULL) {
        return 0;
    }

    *source = NULL;

    file = fopen(filename, "rb");

    if (file == NULL) {
        fprintf(stderr, "Error: cannot open source file '%s'.\n",
                filename);
        return 0;
    }

    buffer = (char *)malloc(capacity);

    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "Error: not enough memory to read source file.\n");
        return 0;
    }

    while (1) {
        if (length + 1 >= capacity) {
            size_t newCapacity;
            char *newBuffer;

            if (capacity > SIZE_MAX / 2) {
                free(buffer);
                fclose(file);
                fprintf(stderr, "Error: source file is too large.\n");
                return 0;
            }

            newCapacity = capacity * 2;

            newBuffer = (char *)realloc(buffer, newCapacity);

            if (newBuffer == NULL) {
                free(buffer);
                fclose(file);
                fprintf(stderr, "Error: not enough memory to read source file.\n");
                return 0;
            }

            buffer = newBuffer;
            capacity = newCapacity;
        }

        bytesRead = fread(
            buffer + length,
            1,
            capacity - length - 1,
            file
        );

        length += bytesRead;

        if (bytesRead == 0) {
            if (ferror(file)) {
                free(buffer);
                fclose(file);
                fprintf(stderr, "Error: failed to read source file '%s'.\n",
                        filename);
                return 0;
            }

            break;
        }
    }

    if (fclose(file) != 0) {
        free(buffer);
        fprintf(stderr, "Error: failed to close source file '%s'.\n",
                filename);
        return 0;
    }

    /*
     * This lexer processes null-terminated source text.
     * Embedded NUL bytes are not supported as source characters.
     */
    if (memchr(buffer, '\0', length) != NULL) {
        free(buffer);
        fprintf(stderr, "Error: source file contains a NUL byte.\n");
        return 0;
    }

    buffer[length] = '\0';
    *source = buffer;

    return 1;
}

/* ---------------------------------------------------------
 * Read a file and tokenize it
 * --------------------------------------------------------- */

int scanFile(const char *filename, TokenArray *tokens)
{
    char *source = NULL;
    Lexer lexer;
    int success;

    if (tokens == NULL) {
        return 0;
    }

    if (tokens->items != NULL || tokens->count != 0
        || tokens->capacity != 0) {
        return 0;
    }

    if (!readSourceFile(filename, &source)) {
        return 0;
    }

    initLexer(&lexer, source);
    success = scanTokens(&lexer, tokens);

    free(source);

    return success;
}
