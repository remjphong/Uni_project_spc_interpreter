#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>

enum TokenType {
    KW_VAR, KW_IF, KW_ELSE, KW_WHILE, KW_RETURN,
    TOKEN_IDENTIFIER, TOKEN_NUMBER,
    OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_MULTIPLY, OPERATOR_DIVIDE,
    OPERATOR_ASSIGN, OPERATOR_EQUAL,
    PUNCT_SEMICOLON, PUNCT_LPAREN, PUNCT_RPAREN,
    TOKEN_EOF, TOKEN_ERROR
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

class ASTNode {
public:
    virtual ~ASTNode() = default; 
};

class BinaryOpNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> left;
    std::string op; 
    std::unique_ptr<ASTNode> right;

    BinaryOpNode(std::unique_ptr<ASTNode> left_node, std::string op_val, std::unique_ptr<ASTNode> right_node);
};

class NumberNode : public ASTNode {
public:
    double value;
    NumberNode(double val);
};

class VariableNode : public ASTNode {
public:
    std::string name;
    VariableNode(std::string var_name);
};

class AssignNode : public ASTNode {
public:
    std::string var_name;
    std::unique_ptr<ASTNode> expr;
    bool is_declaration; // Đánh dấu có từ khóa 'var' hay không

    AssignNode(std::string name, std::unique_ptr<ASTNode> expression, bool is_decl = false);
};

class Parser {
private:
    std::vector<Token> tokens;
    std::size_t current_idx;

public:
    Parser(std::vector<Token> token_list);

    const Token* peek() const;
    const Token* peekNext() const;
    const Token* advance();
    const Token* match(TokenType expected_type);
    const Token* consume(TokenType expected_type, const std::string& error_message);

    // Cú pháp phân cấp
    std::unique_ptr<ASTNode> statement(); 
    std::unique_ptr<ASTNode> expression();
    std::unique_ptr<ASTNode> term();
    std::unique_ptr<ASTNode> factor();
};

void print_ast(const ASTNode* node, std::string indent = "", bool last = true);

#endif