#ifndef LEXER_H
#define LEXER_H

#include "Token.h"
#include <vector>
#include <unordered_map>

class Lexer {
private:
    std::string source;
    size_t cursor = 0;
    int line = 1;
    int column = 1;

    std::unordered_map<std::string, TokenType> keywords;

    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();

public:
    explicit Lexer(std::string src);
    std::vector<Token> scanTokens();
};

#endif // LEXER_H