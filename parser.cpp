#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

using namespace std;

// MÔ PHỎNG LEXER 
struct Token {
    string type;
    string value;

    // Hàm in Token 
    string to_string() const {
        return "Token(type=" + type + ", value=" + value + ")";
    }
};

// CẤU TRÚC DỮ LIỆU AST 

class ASTNode {
public:
    virtual ~ASTNode() = default; 
};

class BinaryOpNode : public ASTNode {
public:
    unique_ptr<ASTNode> left;
    string op; 
    unique_ptr<ASTNode> right;

    BinaryOpNode(unique_ptr<ASTNode> left_node, string op_val, unique_ptr<ASTNode> right_node)
        : left(move(left_node)), op(move(op_val)), right(move(right_node)) {}
};

class NumberNode : public ASTNode {
public:
    double value;

    NumberNode(double val) : value(val) {}
};

class AssignNode : public ASTNode {
public:
    string var_name;
    unique_ptr<ASTNode> expr;

    AssignNode(string name, unique_ptr<ASTNode> expression)
        : var_name(move(name)), expr(move(expression)) {}
};

// xử lý
class Parser {
private:
    vector<Token> tokens;
    size_t current_idx;

public:
    Parser(const vector<Token>& token_list) : tokens(token_list), current_idx(0) {}

    // Lấy token hiện tại (để kiểm tra)
    const Token* peek() {
        if (current_idx < tokens.size()) {
            return &tokens[current_idx];
        }
        return nullptr;
    }

    // Lấy token hiện tại và đẩy con trỏ sang token tiếp theo
    const Token* advance() {
        const Token* token = peek();
        current_idx++;
        return token;
    }

    // Kiểm tra token hiện tại có đúng loại mong muốn không
    const Token* match(const string& expected_type) {
        const Token* token = peek();
        if (token != nullptr && token->type == expected_type) {
            return advance();
        }
        return nullptr;
    }

    // Bắt buộc phải có token loại này, nếu không ném lỗi
    const Token* consume(const string& expected_type, const string& error_message) {
        const Token* token = match(expected_type);
        if (!token) {
            const Token* current = peek();
            string err_token = current ? current->to_string() : "EOF";
            throw runtime_error("Error at token " + err_token + ": " + error_message);
        }
        return token;
    }

    // Xử lý số và dấu ngoặc
    unique_ptr<ASTNode> factor() {
        const Token* token = peek();
        if (!token) throw runtime_error("Unexpected EOF");

        // Nếu là một số -> tạo nút NumberNode
        if (const Token* num_token = match("NUMBER")) {
            return make_unique<NumberNode>(stod(num_token->value)); 
        }

        // Nếu gặp dấu '(' -> Gọi tầng 'expression'
        if (match("LPAREN")) {
            auto node = expression();
            consume("RPAREN", "Expected ')' after expression");
            return node;
        }

        throw runtime_error("Unexpected token: " + token->to_string());
    }

    // Xử lý nhân, chia
    unique_ptr<ASTNode> term() {
        auto node = factor();

        while (const Token* token = peek()) {
            if (token->type == "MULTIPLY" || token->type == "DIVIDE") {
                const Token* op_token = advance();
                auto right_node = factor();
                
                // Bọc node cũ vào BinaryOpNode mới
                node = make_unique<BinaryOpNode>(move(node), op_token->value, move(right_node));
            } else {
                break;
            }
        }
        return node;
    }

    // Xử lý cộng, trừ
    unique_ptr<ASTNode> expression() {
        auto node = term();

        while (const Token* token = peek()) {
            if (token->type == "PLUS" || token->type == "MINUS") {
                const Token* op_token = advance();
                auto right_node = term();
                
                // Bọc node cũ vào BinaryOpNode mới
                node = make_unique<BinaryOpNode>(move(node), op_token->value, move(right_node));
            } else {
                break;
            }
        }
        return node;
    }
};

// HÀM IN CÂY AST
void print_ast(const ASTNode* node, string indent = "", bool last = true) {
    if (!node) return;

    string connector = last ? "└── " : "├── ";

    // Dùng dynamic_cast để kiểm tra kiểu của node
    if (const NumberNode* num_node = dynamic_cast<const NumberNode*>(node)) {
        cout << indent << connector << num_node->value << "\n";
    } 
    else if (const BinaryOpNode* bin_node = dynamic_cast<const BinaryOpNode*>(node)) {
        cout << indent << connector << bin_node->op << "\n";

        string new_indent = indent + (last ? "    " : "│   ");

        // Gọi đệ quy cho nhánh trái và phải
        print_ast(bin_node->left.get(), new_indent, false);
        print_ast(bin_node->right.get(), new_indent, true);
    }
}

int main() {
    // MOCK DATA
    vector<Token> tokens = {
        {"NUMBER", "5"},
        {"PLUS", "+"},
        {"NUMBER", "10"},
        {"MULTIPLY", "*"},
        {"NUMBER", "2"}
    };

    try {
        Parser parser(tokens);
        unique_ptr<ASTNode> ast = parser.expression();

        cout << "\nAST:\n";
        print_ast(ast.get());
    } 
    catch (const exception& e) {
        cerr << "Syntax Error: " << e.what() << "\n";
    }

    return 0;
}