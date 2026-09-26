class ASTNode:
    pass

class BinaryOpNode(ASTNode):
    def __init__(self, left, operator, right):
        self.left = left          
        self.operator = operator  
        self.right = right       

class NumberNode(ASTNode):
    def __init__(self, value):
        self.value = value        

class AssignNode(ASTNode):
    def __init__(self, var_name, expr):
        self.var_name = var_name  
        self.expr = expr

# xử lý
class Parser:
# grammar
    def __init__(self, tokens):
        self.tokens = tokens
        self.current_idx = 0

    # Lấy token hiện tại (để kiểm tra)
    def peek(self):
        if self.current_idx < len(self.tokens):
            return self.tokens[self.current_idx]
        return None

    # Lấy token hiện tại và đẩy con trỏ sang token tiếp theo
    def advance(self):
        token = self.peek()
        self.current_idx += 1
        return token

    # Kiểm tra token hiện tại có đúng loại mong muốn không
    def match(self, expected_type):
        token = self.peek()
        if token and token.type == expected_type:
            return self.advance()
        return None

    def consume(self, expected_type, error_message):
        token = self.match(expected_type)
        if not token:
            raise SyntaxError(f"Error at token {self.peek()}: {error_message}")
        return token

#Recursive Descent

    # số, dấu ngoặc
    def factor(self):
        token = self.peek()

        # Nếu là một số -> tạo nút 
        if self.match("NUMBER"):
            return NumberNode(token.value)
        
        # Nếu gặp dấu '(' -> Gọi tầng 'expression'
        if self.match("LPAREN"):
            node = self.expression()
            self.consume("RPAREN", "Expected ')' after expression")
            return node

        raise SyntaxError(f"Unexpected token: {token}")

    # nhân, chia
    def term(self):
        node = self.factor()

        while self.peek() and self.peek().type in ("MULTIPLY", "DIVIDE"):
            op_token = self.advance()
            right_node = self.factor()
            # Bọc node cũ vào BinaryOpNode mới (đẩy cây phình to dần)
            node = BinaryOpNode(left=node, operator=op_token.value, right=right_node)

        return node

    # cộng, trừ
    def expression(self):
        node = self.term()

        while self.peek() and self.peek().type in ("PLUS", "MINUS"):
            op_token = self.advance()
            right_node = self.term()
            node = BinaryOpNode(left=node, operator=op_token.value, right=right_node)

        return node


# print cây ast 
def print_ast(node, indent="", last=True):
    connector = "└── " if last else "├── "

    if isinstance(node, NumberNode):
        print(indent + connector + str(node.value))

    elif isinstance(node, BinaryOpNode):
        print(indent + connector + node.operator)

        new_indent = indent + ("    " if last else "│   ")

        print_ast(node.left, new_indent, False)
        print_ast(node.right, new_indent, True)

# giả lập lexer
class Token:
    def __init__(self, type, value):
        self.type = type
        self.value = value

    def __repr__(self):
        return f"Token(type={self.type}, value={self.value})"
    
# MOCK DATA
tokens = [
    Token("NUMBER", 5),
    Token("PLUS", "+"),
    Token("NUMBER", 10),
    Token("MULTIPLY", "*"),
    Token("NUMBER", 2)
]

parser = Parser(tokens)

ast = parser.expression();

print("\nAST:")
print_ast(ast)

    