#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>

struct Token {
    std::string type;   
    std::string value;

    std::string to_string() const;
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

// Node cho biến 
class VariableNode : public ASTNode {
public:
    std::string name;
    VariableNode(std::string var_name);
};

// Node cho phép gán 
class AssignNode : public ASTNode {
public:
    std::string var_name;
    std::unique_ptr<ASTNode> expr;

    AssignNode(std::string name, std::unique_ptr<ASTNode> expression);
};

class Parser {
private:
    std::vector<Token> tokens;
    std::size_t current_idx;

public:
    Parser(const std::vector<Token>& token_list);

    const Token* peek();
    const Token* peek_next(); // Nhìn trước 2 token để kiểm tra dấu '='
    const Token* advance();
    const Token* match(const std::string& expected_type);
    const Token* consume(const std::string& expected_type, const std::string& error_message);

    // Điểm bắt đầu parse: có thể là phép gán hoặc một biểu thức
    std::unique_ptr<ASTNode> statement(); 
    std::unique_ptr<ASTNode> factor();
    std::unique_ptr<ASTNode> term();
    std::unique_ptr<ASTNode> expression();
};

void print_ast(const ASTNode* node, std::string indent = "", bool last = true);

#endif