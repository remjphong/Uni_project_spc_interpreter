#ifndef AST_H
#define AST_H

#include <string>
#include <memory>
#include <iostream>
#include <utility>

// Lớp cơ sở trừu tượng cho tất cả các node trong cây AST
class ASTNode {
public:
    virtual ~ASTNode() = default;
};

// Node biểu diễn toán tử nhị phân (+, -, *, /)
class BinaryOpNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> left;
    std::string op;
    std::unique_ptr<ASTNode> right;

    BinaryOpNode(std::unique_ptr<ASTNode> left_node, std::string op_val, std::unique_ptr<ASTNode> right_node)
        : left(std::move(left_node)), op(std::move(op_val)), right(std::move(right_node)) {}
};

// Node biểu diễn hằng số số thực
class NumberNode : public ASTNode {
public:
    double value;

    explicit NumberNode(double val) : value(val) {}
};

// Node biểu diễn biến (đọc giá trị biến)
class VariableNode : public ASTNode {
public:
    std::string name;

    explicit VariableNode(std::string var_name) : name(std::move(var_name)) {}
};

// Node biểu diễn câu lệnh gán hoặc khai báo biến (var x = ...)
class AssignNode : public ASTNode {
public:
    std::string var_name;
    std::unique_ptr<ASTNode> expr;
    bool is_declaration;

    AssignNode(std::string name, std::unique_ptr<ASTNode> expression, bool is_decl = false)
        : var_name(std::move(name)), expr(std::move(expression)), is_declaration(is_decl) {}
};

// Hàm hỗ trợ in cây cú pháp ra màn hình dạng phân nhánh
inline void print_ast(const ASTNode* node, std::string indent = "", bool last = true) {
    if (!node) return;

    std::string connector = last ? "└── " : "├── ";

    if (const auto* num_node = dynamic_cast<const NumberNode*>(node)) {
        std::cout << indent << connector << num_node->value << "\n";
    } 
    else if (const auto* var_node = dynamic_cast<const VariableNode*>(node)) {
        std::cout << indent << connector << "Var(" << var_node->name << ")\n";
    } 
    else if (const auto* assign_node = dynamic_cast<const AssignNode*>(node)) {
        std::cout << indent << connector << (assign_node->is_declaration ? "VarDecl(" : "Assign(") 
                  << assign_node->var_name << " =)\n";

        std::string new_indent = indent + (last ? "    " : "│   ");
        print_ast(assign_node->expr.get(), new_indent, true);
    } 
    else if (const auto* bin_node = dynamic_cast<const BinaryOpNode*>(node)) {
        std::cout << indent << connector << bin_node->op << "\n";

        std::string new_indent = indent + (last ? "    " : "│   ");
        print_ast(bin_node->left.get(), new_indent, false);
        print_ast(bin_node->right.get(), new_indent, true);
    }
}

#endif // AST_H