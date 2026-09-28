#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "Token.h"

class Interpreter;

class Resolver : public ExprVisitor, public StmtVisitor
{
private:
    using Scope = std::unordered_map<std::string, bool>;

    enum class FunctionType
    {
        NONE,
        FUNCTION,
        INITIALIZER,
        METHOD
    };

    enum class ClassType
    {
        NONE,
        CLASS,
        SUBCLASS
    };

    Interpreter& interpreter;
    std::vector<Scope> scopes;

    FunctionType currentFunction = FunctionType::NONE;
    ClassType currentClass = ClassType::NONE;

public:
    explicit Resolver(Interpreter& interpreter);

    // Resolve mot danh sach statement cua chuong trinh/block.
    template <typename StatementContainer>
    void resolve(const StatementContainer& statements)
    {
        for (const auto& statement : statements)
        {
            if (statement)
            {
                resolve(*statement);
            }
        }
    }

    // Resolve mot node.
    void resolve(Stmt& stmt);
    void resolve(Expr& expr);

private:
    // Scope
    void beginScope();
    void endScope();

    // Variable state: false = declared, true = defined.
    void declare(const Token& name);
    void define(const Token& name);

    // Tim declaration gan nhat cua variable va luu lexical distance.
    void resolveLocal(Expr& expr, const Token& name);

    // Resolve function body trong mot scope moi.
    void resolveFunction(const Stmt::Function& function, FunctionType type);

    // In semantic error.
    void error(const Token& token, const std::string& message) const;

    // ----- Expr visitor -----
    void visitAssignExpr(Expr::Assign& expr) override;
    void visitBinaryExpr(Expr::Binary& expr) override;
    void visitCallExpr(Expr::Call& expr) override;
    void visitGetExpr(Expr::Get& expr) override;
    void visitGroupingExpr(Expr::Grouping& expr) override;
    void visitLiteralExpr(Expr::Literal& expr) override;
    void visitLogicalExpr(Expr::Logical& expr) override;
    void visitSetExpr(Expr::Set& expr) override;
    void visitSuperExpr(Expr::Super& expr) override;
    void visitThisExpr(Expr::This& expr) override;
    void visitUnaryExpr(Expr::Unary& expr) override;
    void visitVariableExpr(Expr::Variable& expr) override;

    // ----- Stmt visitor -----
    void visitBlockStmt(Stmt::Block& stmt) override;
    void visitClassStmt(Stmt::Class& stmt) override;
    void visitExpressionStmt(Stmt::Expression& stmt) override;
    void visitFunctionStmt(Stmt::Function& stmt) override;
    void visitIfStmt(Stmt::If& stmt) override;
    void visitPrintStmt(Stmt::Print& stmt) override;
    void visitReturnStmt(Stmt::Return& stmt) override;
    void visitVarStmt(Stmt::Var& stmt) override;
    void visitWhileStmt(Stmt::While& stmt) override;
};
