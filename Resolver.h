#pragma once

#include <any>
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

    void resolve(
        const std::vector<std::shared_ptr<Stmt>>& statements);
        
    // Resolve mot danh sach statement cua chuong trinh/block.
    template <typename StatementContainer>
    void resolveStatements(const StatementContainer& statements)
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

    // Variable state:
    // false = declared
    // true  = defined
    void declare(const Token& name);
    void define(const Token& name);

    // Tim declaration gan nhat cua variable
    // va luu lexical distance.
    void resolveLocal(Expr& expr, const Token& name);

    // Resolve function body trong mot scope moi.
    void resolveFunction(
        const Stmt::Function& function,
        FunctionType type
    );

    // In semantic error.
    void error(
        const Token& token,
        const std::string& message
    ) const;

    // =====================================================
    // Expr visitor
    // ExprVisitor yeu cau std::any.
    // Resolver khong dung gia tri return,
    // nen cac ham se return {}.
    // =====================================================

    std::any visitAssignExpr(Expr::Assign& expr) override;
    std::any visitBinaryExpr(Expr::Binary& expr) override;
    std::any visitCallExpr(Expr::Call& expr) override;
    std::any visitGetExpr(Expr::Get& expr) override;
    std::any visitGroupingExpr(Expr::Grouping& expr) override;
    std::any visitLiteralExpr(Expr::Literal& expr) override;
    std::any visitLogicalExpr(Expr::Logical& expr) override;
    std::any visitSetExpr(Expr::Set& expr) override;
    std::any visitSuperExpr(Expr::Super& expr) override;
    std::any visitThisExpr(Expr::This& expr) override;
    std::any visitUnaryExpr(Expr::Unary& expr) override;
    std::any visitVariableExpr(Expr::Variable& expr) override;

    // =====================================================
    // Stmt visitor
    // StmtVisitor van la void.
    // =====================================================

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