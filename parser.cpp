#include "parser.h"

BinaryOpNode::BinaryOpNode(std::unique_ptr<ASTNode> left_node, std::string op_val, std::unique_ptr<ASTNode> right_node)
    : left(std::move(left_node)), op(std::move(op_val)), right(std::move(right_node)) {}

NumberNode::NumberNode(double val) : value(val) {}

VariableNode::VariableNode(std::string var_name) : name(std::move(var_name)) {}

AssignNode::AssignNode(std::string name, std::unique_ptr<ASTNode> expression, bool is_decl)
    : var_name(std::move(name)), expr(std::move(expression)), is_declaration(is_decl) {}

Parser::Parser(std::vector<Token> token_list) 
    : tokens(std::move(token_list)), current_idx(0) {}

const Token* Parser::peek() const {
    if (current_idx < tokens.size()) {
        return &tokens[current_idx];
    }
    return nullptr;
}

const Token* Parser::peekNext() const {
    if (current_idx + 1 < tokens.size()) {
        return &tokens[current_idx + 1];
    }
    return nullptr;
}

const Token* Parser::advance() {
    const Token* token = peek();
    if (token && token->type != TOKEN_EOF) {
        current_idx++;
    }
    return token;
}

const Token* Parser::match(TokenType expected_type) {
    const Token* token = peek();
    if (token != nullptr && token->type == expected_type) {
        return advance();
    }
    return nullptr;
}

const Token* Parser::consume(TokenType expected_type, const std::string& error_message) {
    const Token* token = match(expected_type);
    if (!token) {
        const Token* current = peek();
        std::string err_pos = current ? ("dong " + std::to_string(current->line) + ", cot " + std::to_string(current->column)) : "EOF";
        throw std::runtime_error("Loi cu phap tai " + err_pos + ": " + error_message);
    }
    return token;
}

// Xử lý câu lệnh: var x = ...; hoặc x = ...; hoặc biểu thức đơn thuần
std::unique_ptr<ASTNode> Parser::statement() {
    bool is_decl = false;

    // Trường hợp 1: Có từ khóa 'var' (ví dụ: var x = 10;)
    if (match(KW_VAR)) {
        is_decl = true;
        const Token* id = consume(TOKEN_IDENTIFIER, "Can ten bien sau tu khoa 'var'");
        consume(OPERATOR_ASSIGN, "Can dau '=' sau ten bien");
        auto expr = expression();
        consume(PUNCT_SEMICOLON, "Thieu dau ';' cuoi cau lenh");
        return std::make_unique<AssignNode>(id->lexeme, std::move(expr), is_decl);
    }

    // Trường hợp 2: Gán lại biến (ví dụ: x = 10;)
    const Token* current = peek();
    const Token* next = peekNext();
    if (current && current->type == TOKEN_IDENTIFIER && next && next->type == OPERATOR_ASSIGN) {
        std::string var_name = advance()->lexeme;
        advance(); // Ăn dấu '='
        auto expr = expression();
        consume(PUNCT_SEMICOLON, "Thieu dau ';' cuoi cau lenh");
        return std::make_unique<AssignNode>(std::move(var_name), std::move(expr), false);
    }

    // Trường hợp 3: Biểu thức thông thường
    auto expr = expression();
    if (peek() && peek()->type == PUNCT_SEMICOLON) {
        advance(); // Bỏ qua dấu ';' nếu có
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::factor() {
    const Token* token = peek();
    if (!token || token->type == TOKEN_EOF) {
        throw std::runtime_error("Ket thuc file dot ngot (Unexpected EOF)");
    }

    // Hằng số (Number)
    if (match(TOKEN_NUMBER)) {
        return std::make_unique<NumberNode>(std::stod(token->lexeme));
    }

    // Tên biến (Identifier)
    if (match(TOKEN_IDENTIFIER)) {
        return std::make_unique<VariableNode>(token->lexeme);
    }

    // Dấu ngoặc tròn ( ... )
    if (match(PUNCT_LPAREN)) {
        auto node = expression();
        consume(PUNCT_RPAREN, "Thieu dau dong ngoac ')' sau bieu thuc");
        return node;
    }

    throw std::runtime_error("Token khong hop le: '" + token->lexeme + "'");
}

std::unique_ptr<ASTNode> Parser::term() {
    auto node = factor();

    while (const Token* token = peek()) {
        if (token->type == OPERATOR_MULTIPLY || token->type == OPERATOR_DIVIDE) {
            const Token* op_token = advance();
            auto right_node = factor();
            node = std::make_unique<BinaryOpNode>(std::move(node), op_token->lexeme, std::move(right_node));
        } else {
            break;
        }
    }
    return node;
}

std::unique_ptr<ASTNode> Parser::expression() {
    auto node = term();

    while (const Token* token = peek()) {
        if (token->type == OPERATOR_PLUS || token->type == OPERATOR_MINUS) {
            const Token* op_token = advance();
            auto right_node = term();
            node = std::make_unique<BinaryOpNode>(std::move(node), op_token->lexeme, std::move(right_node));
        } else {
            break;
        }
    }
    return node;
}

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
        std::cout << indent << connector << (assign_node->is_declaration ? "VarDecl(" : "Assign(") 
                  << assign_node->var_name << " =)\n";

        std::string new_indent = indent + (last ? "    " : "│   ");
        print_ast(assign_node->expr.get(), new_indent, true);
    }
    else if (const BinaryOpNode* bin_node = dynamic_cast<const BinaryOpNode*>(node)) {
        std::cout << indent << connector << bin_node->op << "\n";

        std::string new_indent = indent + (last ? "    " : "│   ");
        print_ast(bin_node->left.get(), new_indent, false);
        print_ast(bin_node->right.get(), new_indent, true);
    }
}