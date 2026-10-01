#pragma once

#include <any>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include "Token.h"

class ExprVisitor;
class StmtVisitor;

class Expr {
public:
    struct Assign;
    struct Binary;
    struct Call;
    struct Get;
    struct Grouping;
    struct Literal;
    struct Logical;
    struct Set;
    struct Super;
    struct This;
    struct Unary;
    struct Variable;

    virtual ~Expr() = default;
    virtual std::any accept(ExprVisitor&) = 0;
};

class Stmt {
public:
    struct Block;
    struct Class;
    struct Expression;
    struct Function;
    struct If;
    struct Print;
    struct Return;
    struct Var;
    struct While;

    virtual ~Stmt() = default;
    virtual void accept(StmtVisitor&) = 0;
};

class ExprVisitor {
public:
    virtual ~ExprVisitor() = default;

    virtual std::any visitAssignExpr(Expr::Assign&) = 0;
    virtual std::any visitBinaryExpr(Expr::Binary&) = 0;
    virtual std::any visitCallExpr(Expr::Call&) = 0;
    virtual std::any visitGetExpr(Expr::Get&) = 0;
    virtual std::any visitGroupingExpr(Expr::Grouping&) = 0;
    virtual std::any visitLiteralExpr(Expr::Literal&) = 0;
    virtual std::any visitLogicalExpr(Expr::Logical&) = 0;
    virtual std::any visitSetExpr(Expr::Set&) = 0;
    virtual std::any visitSuperExpr(Expr::Super&) = 0;
    virtual std::any visitThisExpr(Expr::This&) = 0;
    virtual std::any visitUnaryExpr(Expr::Unary&) = 0;
    virtual std::any visitVariableExpr(Expr::Variable&) = 0;
};

class StmtVisitor {
public:
    virtual ~StmtVisitor() = default;

    virtual void visitBlockStmt(Stmt::Block&) = 0;
    virtual void visitClassStmt(Stmt::Class&) = 0;
    virtual void visitExpressionStmt(Stmt::Expression&) = 0;
    virtual void visitFunctionStmt(Stmt::Function&) = 0;
    virtual void visitIfStmt(Stmt::If&) = 0;
    virtual void visitPrintStmt(Stmt::Print&) = 0;
    virtual void visitReturnStmt(Stmt::Return&) = 0;
    virtual void visitVarStmt(Stmt::Var&) = 0;
    virtual void visitWhileStmt(Stmt::While&) = 0;
};

// Expr

struct Expr::Assign : Expr {
    Token name;
    std::shared_ptr<Expr> value;

    Assign(Token n, std::shared_ptr<Expr> v)
        : name(std::move(n)), value(std::move(v)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitAssignExpr(*this);
    }
};

struct Expr::Binary : Expr {
    std::shared_ptr<Expr> left;
    Token op;
    std::shared_ptr<Expr> right;

    Binary(std::shared_ptr<Expr> l, Token o, std::shared_ptr<Expr> r)
        : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitBinaryExpr(*this);
    }
};

struct Expr::Call : Expr {
    std::shared_ptr<Expr> callee;
    Token paren;
    std::vector<std::shared_ptr<Expr>> arguments;

    Call(
        std::shared_ptr<Expr> c,
        Token p,
        std::vector<std::shared_ptr<Expr>> a
    ) : callee(std::move(c)), paren(std::move(p)), arguments(std::move(a)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitCallExpr(*this);
    }
};

struct Expr::Get : Expr {
    std::shared_ptr<Expr> object;
    Token name;

    Get(std::shared_ptr<Expr> o, Token n)
        : object(std::move(o)), name(std::move(n)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitGetExpr(*this);
    }
};

struct Expr::Grouping : Expr {
    std::shared_ptr<Expr> expression;

    explicit Grouping(std::shared_ptr<Expr> e)
        : expression(std::move(e)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitGroupingExpr(*this);
    }
};

struct Expr::Literal : Expr {
    std::any value;

    explicit Literal(std::any v)
        : value(std::move(v)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitLiteralExpr(*this);
    }
};

struct Expr::Logical : Expr {
    std::shared_ptr<Expr> left;
    Token op;
    std::shared_ptr<Expr> right;

    Logical(
        std::shared_ptr<Expr> l,
        Token o,
        std::shared_ptr<Expr> r
    ) : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitLogicalExpr(*this);
    }
};

struct Expr::Set : Expr {
    std::shared_ptr<Expr> object;
    Token name;
    std::shared_ptr<Expr> value;

    Set(
        std::shared_ptr<Expr> o,
        Token n,
        std::shared_ptr<Expr> v
    ) : object(std::move(o)), name(std::move(n)), value(std::move(v)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitSetExpr(*this);
    }
};

struct Expr::Super : Expr {
    Token keyword;
    Token method;

    Super(Token k, Token m)
        : keyword(std::move(k)), method(std::move(m)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitSuperExpr(*this);
    }
};

struct Expr::This : Expr {
    Token keyword;

    explicit This(Token k)
        : keyword(std::move(k)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitThisExpr(*this);
    }
};

struct Expr::Unary : Expr {
    Token op;
    std::shared_ptr<Expr> right;

    Unary(Token o, std::shared_ptr<Expr> r)
        : op(std::move(o)), right(std::move(r)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitUnaryExpr(*this);
    }
};

struct Expr::Variable : Expr {
    Token name;

    explicit Variable(Token n)
        : name(std::move(n)) {}

    std::any accept(ExprVisitor& v) override {
        return v.visitVariableExpr(*this);
    }
};

// Stmt

struct Stmt::Block : Stmt {
    std::vector<std::shared_ptr<Stmt>> statements;

    explicit Block(std::vector<std::shared_ptr<Stmt>> s)
        : statements(std::move(s)) {}

    void accept(StmtVisitor& v) override {
        v.visitBlockStmt(*this);
    }
};

struct Stmt::Class : Stmt {
    Token name;
    std::shared_ptr<Expr::Variable> superclass;
    std::vector<Stmt::Function> methods;

    Class(
        Token n,
        std::shared_ptr<Expr::Variable> s,
        std::vector<Stmt::Function> m
    ) : name(std::move(n)),
        superclass(std::move(s)),
        methods(std::move(m)) {}

    void accept(StmtVisitor& v) override {
        v.visitClassStmt(*this);
    }
};

struct Stmt::Expression : Stmt {
    std::shared_ptr<Expr> expression;

    explicit Expression(std::shared_ptr<Expr> e)
        : expression(std::move(e)) {}

    void accept(StmtVisitor& v) override {
        v.visitExpressionStmt(*this);
    }
};

struct Stmt::Function : Stmt {
    Token name;
    std::vector<Token> params;
    std::vector<std::shared_ptr<Stmt>> body;

    Function(
        Token n,
        std::vector<Token> p,
        std::vector<std::shared_ptr<Stmt>> b
    ) : name(std::move(n)),
        params(std::move(p)),
        body(std::move(b)) {}

    void accept(StmtVisitor& v) override {
        v.visitFunctionStmt(*this);
    }
};

struct Stmt::If : Stmt {
    std::shared_ptr<Expr> condition;
    std::shared_ptr<Stmt> thenBranch;
    std::shared_ptr<Stmt> elseBranch;

    If(
        std::shared_ptr<Expr> c,
        std::shared_ptr<Stmt> t,
        std::shared_ptr<Stmt> e
    ) : condition(std::move(c)),
        thenBranch(std::move(t)),
        elseBranch(std::move(e)) {}

    void accept(StmtVisitor& v) override {
        v.visitIfStmt(*this);
    }
};

struct Stmt::Print : Stmt {
    std::shared_ptr<Expr> expression;

    explicit Print(std::shared_ptr<Expr> e)
        : expression(std::move(e)) {}

    void accept(StmtVisitor& v) override {
        v.visitPrintStmt(*this);
    }
};

struct Stmt::Return : Stmt {
    Token keyword;
    std::shared_ptr<Expr> value;

    Return(Token k, std::shared_ptr<Expr> v)
        : keyword(std::move(k)), value(std::move(v)) {}

    void accept(StmtVisitor& v) override {
        v.visitReturnStmt(*this);
    }
};

struct Stmt::Var : Stmt {
    Token name;
    std::shared_ptr<Expr> initializer;

    Var(Token n, std::shared_ptr<Expr> i)
        : name(std::move(n)), initializer(std::move(i)) {}

    void accept(StmtVisitor& v) override {
        v.visitVarStmt(*this);
    }
};

struct Stmt::While : Stmt {
    std::shared_ptr<Expr> condition;
    std::shared_ptr<Stmt> body;

    While(std::shared_ptr<Expr> c, std::shared_ptr<Stmt> b)
        : condition(std::move(c)), body(std::move(b)) {}

    void accept(StmtVisitor& v) override {
        v.visitWhileStmt(*this);
    }
};