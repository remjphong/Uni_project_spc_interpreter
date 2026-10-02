#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "AST.h"
#include "Token.h"

class Parser {
private:
    struct ParseError {};

    std::vector<Token> tokens;
    std::size_t current = 0;

    bool isAtEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(std::initializer_list<TokenType> types);
    const Token& consume(
        TokenType type,
        const std::string& message);

    ParseError error(
        const Token& token,
        const std::string& message);

    void synchronize();

    std::shared_ptr<Stmt> declaration();
    std::shared_ptr<Stmt> classDeclaration();
    std::shared_ptr<Stmt::Function> function(
        const std::string& kind);
    std::shared_ptr<Stmt> varDeclaration();

    std::shared_ptr<Stmt> statement();
    std::shared_ptr<Stmt> forStatement();
    std::shared_ptr<Stmt> ifStatement();
    std::shared_ptr<Stmt> printStatement();
    std::shared_ptr<Stmt> returnStatement();
    std::shared_ptr<Stmt> whileStatement();
    std::vector<std::shared_ptr<Stmt>> block();

    std::shared_ptr<Expr> expression();
    std::shared_ptr<Expr> assignment();
    std::shared_ptr<Expr> logicOr();
    std::shared_ptr<Expr> logicAnd();
    std::shared_ptr<Expr> equality();
    std::shared_ptr<Expr> comparison();
    std::shared_ptr<Expr> term();
    std::shared_ptr<Expr> factor();
    std::shared_ptr<Expr> unary();
    std::shared_ptr<Expr> call();
    std::shared_ptr<Expr> finishCall(
        std::shared_ptr<Expr> callee);
    std::shared_ptr<Expr> primary();

public:
    explicit Parser(std::vector<Token> token_list);

    std::vector<std::shared_ptr<Stmt>> parse();
};