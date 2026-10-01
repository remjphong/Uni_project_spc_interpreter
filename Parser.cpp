#include "Parser.h"

#include <iostream>
#include <stdexcept>
#include <utility>

Parser::Parser(std::vector<Token> token_list)
    : tokens(std::move(token_list)) {}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) ++current;
    return previous();
}

bool Parser::check(TokenType type) const {
    return isAtEnd() ? type == TokenType::END_OF_FILE : peek().type == type;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw error(peek(), message);
}

Parser::ParseError Parser::error(const Token& token, const std::string& message) {
    std::cerr << "[line " << token.line << "] Error";
    if (token.type == TokenType::END_OF_FILE)
        std::cerr << " at end";
    else
        std::cerr << " at '" << token.lexeme << "'";
    std::cerr << ": " << message << '\n';
    return {};
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;

        switch (peek().type) {
        case TokenType::CLASS:
        case TokenType::FUN:
        case TokenType::VAR:
        case TokenType::FOR:
        case TokenType::IF:
        case TokenType::WHILE:
        case TokenType::PRINT:
        case TokenType::RETURN:
        case TokenType::RIGHT_BRACE:
            return;
        default:
            advance();
        }
    }
}

// Parse

std::vector<std::shared_ptr<Stmt>> Parser::parse() {
    std::vector<std::shared_ptr<Stmt>> statements;
    while (!isAtEnd()) {
        auto stmt = declaration();
        if (stmt) statements.push_back(std::move(stmt));
    }
    return statements;
}

// Declaration

std::shared_ptr<Stmt> Parser::declaration() {
    try {
        if (match({TokenType::CLASS})) return classDeclaration();
        if (match({TokenType::FUN})) return function("function");
        if (match({TokenType::VAR})) return varDeclaration();
        return statement();
    } catch (const ParseError&) {
        synchronize();
        return nullptr;
    }
}

std::shared_ptr<Stmt> Parser::classDeclaration() {
    Token name = consume(TokenType::IDENTIFIER, "Expect class name.");
    std::shared_ptr<Expr::Variable> superclass;

    if (match({TokenType::LESS})) {
        Token superName = consume(
            TokenType::IDENTIFIER, "Expect superclass name.");
        superclass = std::make_shared<Expr::Variable>(superName);
    }

    consume(
        TokenType::LEFT_BRACE,
        "Expect '{' before class body.");

    std::vector<Stmt::Function> methods;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto method = function("method");
        methods.push_back(
            *std::dynamic_pointer_cast<Stmt::Function>(method));
    }

    consume(
        TokenType::RIGHT_BRACE,
        "Expect '}' after class body.");

    return std::make_shared<Stmt::Class>(
        name, std::move(superclass), std::move(methods));
}

std::shared_ptr<Stmt> Parser::function(const std::string& kind) {
    Token name = consume(
        TokenType::IDENTIFIER, "Expect " + kind + " name.");

    consume(TokenType::LEFT_PAREN, "Expect '(' after name.");

    std::vector<Token> params;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            params.push_back(consume(
                TokenType::IDENTIFIER, "Expect parameter name."));
        } while (match({TokenType::COMMA}));
    }

    consume(
        TokenType::RIGHT_PAREN,
        "Expect ')' after parameters.");

    consume(
        TokenType::LEFT_BRACE,
        "Expect '{' before body.");

    return std::make_shared<Stmt::Function>(
        name, std::move(params), block());
}

std::shared_ptr<Stmt> Parser::varDeclaration() {
    Token name = consume(
        TokenType::IDENTIFIER, "Expect variable name.");

    std::shared_ptr<Expr> initializer;
    if (match({TokenType::EQUAL})) initializer = expression();

    consume(
        TokenType::SEMICOLON,
        "Expect ';' after variable declaration.");

    return std::make_shared<Stmt::Var>(
        name, std::move(initializer));
}

// Statement

std::shared_ptr<Stmt> Parser::statement() {
    if (match({TokenType::FOR})) return forStatement();
    if (match({TokenType::IF})) return ifStatement();
    if (match({TokenType::PRINT})) return printStatement();
    if (match({TokenType::RETURN})) return returnStatement();
    if (match({TokenType::WHILE})) return whileStatement();
    if (match({TokenType::LEFT_BRACE}))
        return std::make_shared<Stmt::Block>(block());

    auto expr = expression();
    consume(TokenType::SEMICOLON, "Expect ';' after expression.");

    return std::make_shared<Stmt::Expression>(std::move(expr));
}

std::shared_ptr<Stmt> Parser::forStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");

    std::shared_ptr<Stmt> initializer;
    if (match({TokenType::SEMICOLON})) {
        initializer = nullptr;
    } else if (match({TokenType::VAR})) {
        initializer = varDeclaration();
    } else {
        auto expr = expression();
        consume(
            TokenType::SEMICOLON,
            "Expect ';' after loop initializer.");
        initializer = std::make_shared<Stmt::Expression>(
            std::move(expr));
    }

    std::shared_ptr<Expr> condition;
    if (!check(TokenType::SEMICOLON)) condition = expression();

    consume(
        TokenType::SEMICOLON,
        "Expect ';' after loop condition.");

    std::shared_ptr<Expr> increment;
    if (!check(TokenType::RIGHT_PAREN)) increment = expression();

    consume(
        TokenType::RIGHT_PAREN,
        "Expect ')' after clauses.");

    auto body = statement();

    if (increment) {
        std::vector<std::shared_ptr<Stmt>> statements{
            body,
            std::make_shared<Stmt::Expression>(std::move(increment))
        };
        body = std::make_shared<Stmt::Block>(std::move(statements));
    }

    if (!condition)
        condition = std::make_shared<Expr::Literal>(true);

    body = std::make_shared<Stmt::While>(
        std::move(condition), std::move(body));

    if (initializer) {
        std::vector<std::shared_ptr<Stmt>> statements{
            std::move(initializer), std::move(body)
        };
        body = std::make_shared<Stmt::Block>(std::move(statements));
    }

    return body;
}

std::shared_ptr<Stmt> Parser::ifStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
    auto condition = expression();
    consume(
        TokenType::RIGHT_PAREN,
        "Expect ')' after condition.");

    auto thenBranch = statement();
    std::shared_ptr<Stmt> elseBranch;

    if (match({TokenType::ELSE})) elseBranch = statement();

    return std::make_shared<Stmt::If>(
        std::move(condition),
        std::move(thenBranch),
        std::move(elseBranch));
}

std::shared_ptr<Stmt> Parser::printStatement() {
    auto expr = expression();
    consume(TokenType::SEMICOLON, "Expect ';' after value.");
    return std::make_shared<Stmt::Print>(std::move(expr));
}

std::shared_ptr<Stmt> Parser::returnStatement() {
    Token keyword = previous();
    std::shared_ptr<Expr> value;

    if (!check(TokenType::SEMICOLON)) value = expression();

    consume(
        TokenType::SEMICOLON,
        "Expect ';' after return value.");

    return std::make_shared<Stmt::Return>(
        keyword, std::move(value));
}

std::shared_ptr<Stmt> Parser::whileStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
    auto condition = expression();
    consume(
        TokenType::RIGHT_PAREN,
        "Expect ')' after condition.");

    return std::make_shared<Stmt::While>(
        std::move(condition), statement());
}

std::vector<std::shared_ptr<Stmt>> Parser::block() {
    std::vector<std::shared_ptr<Stmt>> statements;

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto stmt = declaration();
        if (stmt) statements.push_back(std::move(stmt));
    }

    consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
    return statements;
}

// Expression

std::shared_ptr<Expr> Parser::expression() {
    return assignment();
}

std::shared_ptr<Expr> Parser::assignment() {
    auto expr = logicOr();
    if (!match({TokenType::EQUAL})) return expr;

    auto value = assignment();

    if (auto variable =
        std::dynamic_pointer_cast<Expr::Variable>(expr)) {
        return std::make_shared<Expr::Assign>(
            variable->name, std::move(value));
    }

    if (auto get =
        std::dynamic_pointer_cast<Expr::Get>(expr)) {
        return std::make_shared<Expr::Set>(
            get->object, get->name, std::move(value));
    }

    throw error(previous(), "Invalid assignment target.");
}

std::shared_ptr<Expr> Parser::logicOr() {
    auto expr = logicAnd();

    while (match({TokenType::OR})) {
        Token op = previous();
        expr = std::make_shared<Expr::Logical>(
            std::move(expr), std::move(op), logicAnd());
    }

    return expr;
}

std::shared_ptr<Expr> Parser::logicAnd() {
    auto expr = equality();

    while (match({TokenType::AND})) {
        Token op = previous();
        expr = std::make_shared<Expr::Logical>(
            std::move(expr), std::move(op), equality());
    }

    return expr;
}

std::shared_ptr<Expr> Parser::equality() {
    auto expr = comparison();

    while (match({
        TokenType::BANG_EQUAL,
        TokenType::EQUAL_EQUAL
    })) {
        Token op = previous();
        expr = std::make_shared<Expr::Binary>(
            std::move(expr), std::move(op), comparison());
    }

    return expr;
}

std::shared_ptr<Expr> Parser::comparison() {
    auto expr = term();

    while (match({
        TokenType::GREATER,
        TokenType::GREATER_EQUAL,
        TokenType::LESS,
        TokenType::LESS_EQUAL
    })) {
        Token op = previous();
        expr = std::make_shared<Expr::Binary>(
            std::move(expr), std::move(op), term());
    }

    return expr;
}

std::shared_ptr<Expr> Parser::term() {
    auto expr = factor();

    while (match({
        TokenType::MINUS,
        TokenType::PLUS
    })) {
        Token op = previous();
        expr = std::make_shared<Expr::Binary>(
            std::move(expr), std::move(op), factor());
    }

    return expr;
}

std::shared_ptr<Expr> Parser::factor() {
    auto expr = unary();

    while (match({
        TokenType::SLASH,
        TokenType::STAR
    })) {
        Token op = previous();
        expr = std::make_shared<Expr::Binary>(
            std::move(expr), std::move(op), unary());
    }

    return expr;
}

std::shared_ptr<Expr> Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS})) {
        Token op = previous();
        return std::make_shared<Expr::Unary>(
            std::move(op), unary());
    }

    return call();
}

std::shared_ptr<Expr> Parser::call() {
    auto expr = primary();

    while (true) {
        if (match({TokenType::LEFT_PAREN})) {
            expr = finishCall(std::move(expr));
        } else if (match({TokenType::DOT})) {
            Token name = consume(
                TokenType::IDENTIFIER,
                "Expect property name.");
            expr = std::make_shared<Expr::Get>(
                std::move(expr), std::move(name));
        } else {
            break;
        }
    }

    return expr;
}

std::shared_ptr<Expr> Parser::finishCall(
    std::shared_ptr<Expr> callee) {
    std::vector<std::shared_ptr<Expr>> arguments;

    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            arguments.push_back(expression());
        } while (match({TokenType::COMMA}));
    }

    Token paren = consume(
        TokenType::RIGHT_PAREN,
        "Expect ')' after arguments.");

    return std::make_shared<Expr::Call>(
        std::move(callee),
        std::move(paren),
        std::move(arguments));
}

std::shared_ptr<Expr> Parser::primary() {
    if (match({TokenType::FALSE}))
        return std::make_shared<Expr::Literal>(false);

    if (match({TokenType::TRUE}))
        return std::make_shared<Expr::Literal>(true);

    if (match({TokenType::NIL}))
        return std::make_shared<Expr::Literal>(std::any{});

    if (match({TokenType::NUMBER, TokenType::STRING}))
        return std::make_shared<Expr::Literal>(previous().literal);

    if (match({TokenType::THIS}))
        return std::make_shared<Expr::This>(previous());

    if (match({TokenType::SUPER})) {
        Token keyword = previous();
        consume(
            TokenType::DOT,
            "Expect '.' after 'super'.");
        Token method = consume(
            TokenType::IDENTIFIER,
            "Expect superclass method name.");

        return std::make_shared<Expr::Super>(
            std::move(keyword), std::move(method));
    }

    if (match({TokenType::IDENTIFIER}))
        return std::make_shared<Expr::Variable>(previous());

    if (match({TokenType::LEFT_PAREN})) {
        auto expr = expression();
        consume(
            TokenType::RIGHT_PAREN,
            "Expect ')' after expression.");
        return std::make_shared<Expr::Grouping>(std::move(expr));
    }

    throw error(peek(), "Expect expression.");
}