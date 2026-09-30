#include "parser.h"

// Token 
std::string Token::to_string() const {
    return "Token(type=" + type + ", value=" + value + ")";
}

// AST Nodes 
BinaryOpNode::BinaryOpNode(std::unique_ptr<ASTNode> left_node, std::string op_val, std::unique_ptr<ASTNode> right_node)
    : left(std::move(left_node)), op(std::move(op_val)), right(std::move(right_node)) {}

NumberNode::NumberNode(double val) : value(val) {}

VariableNode::VariableNode(std::string var_name) : name(std::move(var_name)) {}

AssignNode::AssignNode(std::string name, std::unique_ptr<ASTNode> expression)
    : var_name(std::move(name)), expr(std::move(expression)) {}

// Parser 
Parser::Parser(const std::vector<Token>& token_list) : tokens(token_list), current_idx(0) {}

const Token* Parser::peek() {
    if (current_idx < tokens.size()) {
        return &tokens[current_idx];
    }
    return nullptr;
}

// Hàm nhìn trước 1 token phía sau (Lookahead = 2)
const Token* Parser::peek_next() {
    if (current_idx + 1 < tokens.size()) {
        return &tokens[current_idx + 1];
    }
    return nullptr;
}

const Token* Parser::advance() {
    const Token* token = peek();
    if (token) {
        current_idx++;
    }
    return token;
}

const Token* Parser::match(const std::string& expected_type) {
    const Token* token = peek();
    if (token != nullptr && token->type == expected_type) {
        return advance();
    }
    return nullptr;
}

const Token* Parser::consume(const std::string& expected_type, const std::string& error_message) {
    const Token* token = match(expected_type);
    if (!token) {
        const Token* current = peek();
        std::string err_token = current ? current->to_string() : "EOF";
        throw std::runtime_error("Error at token " + err_token + ": " + error_message);
    }
    return token;
}

// Parse phép gán 
std::unique_ptr<ASTNode> Parser::statement() {
    const Token* current = peek();
    const Token* next = peek_next();

    if (current && current->type == "IDENTIFIER" && next && next->type == "ASSIGN") {
        std::string var_name = advance()->value; // Lấy tên biến
        advance();                              // Bỏ qua dấu '='
        auto expr = expression();               // Parse biểu thức bên phải
        return std::make_unique<AssignNode>(std::move(var_name), std::move(expr));
    }

    // Nếu không phải phép gán, coi nó là biểu thức thông thường
    return expression();
}

std::unique_ptr<ASTNode> Parser::factor() {
    const Token* token = peek();
    if (!token) throw std::runtime_error("Unexpected EOF");

    // Xử lý số
    if (const Token* num_token = match("NUMBER")) {
        return std::make_unique<NumberNode>(std::stod(num_token->value)); 
    }

    // Xử lý biến trong biểu thức 
    if (const Token* id_token = match("IDENTIFIER")) {
        return std::make_unique<VariableNode>(id_token->value);
    }

    // Xử lý dấu ngoặc ()
    if (match("LPAREN")) {
        auto node = expression();
        consume("RPAREN", "Expected ')' after expression");
        return node;
    }
    // xử lý dấu trừ ở đầu
    if (match("MINUS")) {
        auto right = factor();
        return std::make_unique<BinaryOpNode>(
            std::make_unique<NumberNode>(0), "-", std::move(right)
        );
    }

    throw std::runtime_error("Unexpected token: " + token->to_string());
}

std::unique_ptr<ASTNode> Parser::term() {
    auto node = factor();

    while (const Token* token = peek()) {
        if (token->type == "MULTIPLY" || token->type == "DIVIDE") {
            const Token* op_token = advance();
            auto right_node = factor();
            
            node = std::make_unique<BinaryOpNode>(std::move(node), op_token->value, std::move(right_node));
        } else {
            break;
        }
    }
    return node;
}

std::unique_ptr<ASTNode> Parser::expression() {
    auto node = term();

    while (const Token* token = peek()) {
        if (token->type == "PLUS" || token->type == "MINUS") {
            const Token* op_token = advance();
            auto right_node = term();
            
            node = std::make_unique<BinaryOpNode>(std::move(node), op_token->value, std::move(right_node));
        } else {
            break;
        }
    }
    return node;
}

// in cây ast
void print_ast(const ASTNode* node, std::string indent, bool last) {
    if (!node) return;

    std::string connector = last ? "└── " : "├── ";

    if (const NumberNode* num_node = dynamic_cast<const NumberNode*>(node)) {
        std::cout << indent << connector << num_node->value << "\n";
    } 
    else if (const VariableNode* var_node = dynamic_cast<const VariableNode*>(node)) {
        std::cout << indent << connector << "Var(" << var_node->name << ")\n";
    }
    else if (const AssignNode* assign_node = dynamic_cast<const AssignNode*>(node)) {
        std::cout << indent << connector << "Assign(" << assign_node->var_name << " =)\n";

        std::string new_indent = indent + (last ? "    " : "│   ");
        // Biểu thức gán chỉ có 1 nhánh con bên phải (expr)
        print_ast(assign_node->expr.get(), new_indent, true);
    }
    else if (const BinaryOpNode* bin_node = dynamic_cast<const BinaryOpNode*>(node)) {
        std::cout << indent << connector << bin_node->op << "\n";

        std::string new_indent = indent + (last ? "    " : "│   ");

        print_ast(bin_node->left.get(), new_indent, false);
        print_ast(bin_node->right.get(), new_indent, true);
    }
}