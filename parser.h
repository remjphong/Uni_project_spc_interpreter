#ifndef PARSER_H
#define PARSER_H

#include "Token.h"
#include "AST.h"
#include <vector>
#include <memory>
#include <string>
#include <cstddef>

class Parser {
private:
    std::vector<Token> tokens;
    std::size_t current_idx;

public:
    explicit Parser(std::vector<Token> token_list);

    // Các hàm tiện ích duyệt token
    const Token* peek() const;
    const Token* peekNext() const;
    const Token* advance();
    const Token* match(TokenType expected_type);
    const Token* consume(TokenType expected_type, const std::string& error_message);

    // Ngữ pháp phân cấp (Recursive Descent Parsing)
    std::unique_ptr<ASTNode> statement(); 
    std::unique_ptr<ASTNode> expression();
    std::unique_ptr<ASTNode> term();
    std::unique_ptr<ASTNode> factor();
};

#endif // PARSER_H