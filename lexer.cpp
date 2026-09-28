#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>

// 1. ĐỊNH NGHĨA CÁC LOẠI TOKEN (TokenType)
enum TokenType {
    // Keywords
    KW_VAR, KW_IF, KW_ELSE, KW_WHILE, KW_RETURN,
    
    // Literals & Identifiers
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    
    // Operators & Punctuators
    OPERATOR_PLUS,      // +
    OPERATOR_MINUS,     // -
    OPERATOR_ASSIGN,    // =
    OPERATOR_EQUAL,     // ==
    PUNCT_SEMICOLON,   // ;
    
    // Special
    TOKEN_EOF,
    TOKEN_ERROR
};

// 2. CẤU TRÚC TOKEN BÀN GIAO CHO PARSER
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    // Hàm phụ trợ để in thông tin Token ra màn hình
    void print() const {
        std::cout << "Token [Type: " << type 
                  << ", Lexeme: '" << lexeme << "'"
                  << ", Line: " << line 
                  << ", Col: " << column << "]\n";
    }
};

// 3. LỚP LEXER CHÍNH
class Lexer {
private:
    std::string source;
    size_t cursor = 0;
    int line = 1;
    int column = 1;

    // KEYWORD TABLE (Bảng từ khóa tra cứu O(1))
    std::unordered_map<std::string, TokenType> keywords = {
        {"var", KW_VAR},
        {"if", KW_IF},
        {"else", KW_ELSE},
        {"while", KW_WHILE},
        {"return", KW_RETURN}
    };

    // Hàm phụ trợ kiêm tra xem đã hết chuỗi chưa
    bool isAtEnd() const {
        return cursor >= source.length();
    }

    // Nhìn trước ký tự hiện tại (Peek)
    char peek() const {
        if (isAtEnd()) return '\0';
        return source[cursor];
    }

    // Nhìn trước ký tự tiếp theo (Lookahead)
    char peekNext() const {
        if (cursor + 1 >= source.length()) return '\0';
        return source[cursor + 1];
    }

    // Đọc ký tự hiện tại và dịch con trỏ (Advance)
    char advance() {
        char c = source[cursor++];
        column++;
        return c;
    }

public:
    Lexer(std::string src) : source(src) {}

    // Hàm chính: Quét và chuyển đổi toàn bộ Input thành Token Stream
    std::vector<Token> scanTokens() {
        std::vector<Token> tokens;

        while (!isAtEnd()) {
            char c = peek();

            // --- BỎ QUA TRIVIA (Khoảng trắng, Xuống dòng) ---
            if (c == ' ' || c == '\t' || c == '\r') {
                advance();
            } 
            else if (c == '\n') {
                line++;
                column = 1;
                advance();
            }
            
            // --- DFA STATE: READ_ID & KEYWORD LOOKUP (Chữ cái / _) ---
            else if (isalpha(c) || c == '_') {
                int startCol = column;
                std::string acc = "";

                while (isalnum(peek()) || peek() == '_') {
                    acc += advance();
                }

                // Tra bảng Keyword Table
                TokenType type = TOKEN_IDENTIFIER;
                if (keywords.find(acc) != keywords.end()) {
                    type = keywords[acc]; // Tìm thấy trong Bảng từ khóa
                }

                tokens.push_back({type, acc, line, startCol});
            }

            // --- DFA STATE: READ_NUMBER (Chữ số 0-9) ---
            else if (isdigit(c)) {
                int startCol = column;
                std::string acc = "";

                while (isdigit(peek())) {
                    acc += advance();
                }

                tokens.push_back({TOKEN_NUMBER, acc, line, startCol});
            }

            // --- DFA STATE: READ_OPERATOR & PUNCTUATORS ---
            else if (c == '=') {
                int startCol = column;
                advance(); // Ăn dấu '=' đầu tiên

                // Lookahead: Kiểm tra xem có phải '==' không
                if (peek() == '=') {
                    advance(); // Ăn dấu '=' thứ hai
                    tokens.push_back({OPERATOR_EQUAL, "==", line, startCol});
                } else {
                    tokens.push_back({OPERATOR_ASSIGN, "=", line, startCol});
                }
            }
            else if (c == '+') {
                int startCol = column;
                advance();
                tokens.push_back({OPERATOR_PLUS, "+", line, startCol});
            }
            else if (c == '-') {
                int startCol = column;
                advance();
                tokens.push_back({OPERATOR_MINUS, "-", line, startCol});
            }
            else if (c == ';') {
                int startCol = column;
                advance();
                tokens.push_back({PUNCT_SEMICOLON, ";", line, startCol});
            }

            // --- XỬ LÝ LỖI (Ký tự không hợp lệ) ---
            else {
                int startCol = column;
                std::string errStr(1, advance());
                std::cerr << "Loi Lexer: Ky tu khong hop le '" << errStr 
                          << "' tai dong " << line << ", cot " << startCol << "\n";
                tokens.push_back({TOKEN_ERROR, errStr, line, startCol});
            }
        }

        // Kéo cờ kết thúc file (EOF)
        tokens.push_back({TOKEN_EOF, "", line, column});
        return tokens;
    }
};

// 4. CHƯƠNG TRÌNH CHẠY THỬ (MAIN)
int main() {
    // Đoạn code mẫu đầu vào
    std::string code = "var x = 10;\nif x == 10 return;";

    std::cout << "--- CODE INPUT ---\n" << code << "\n\n";
    std::cout << "--- TOKEN STREAM DONG GOI CHO PARSER ---\n";

    Lexer lexer(code);
    std::vector<Token> tokenStream = lexer.scanTokens();

    for (const auto& token : tokenStream) {
        token.print();
    }

    return 0;
}
