#pragma once

#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "Environment.h"
#include "Token.h"

class Interpreter;

class LoxCallable
{
public:
    virtual ~LoxCallable() = default;

    virtual int arity() const = 0;

    virtual std::any call(
        Interpreter& interpreter,
        const std::vector<std::any>& arguments
    ) = 0;

    virtual std::string toString() const = 0;
};


class Interpreter
    : public ExprVisitor,
      public StmtVisitor
{
private:
    std::shared_ptr<Environment> globals;
    std::shared_ptr<Environment> environment;

    std::unordered_map<Expr*, int> locals;

public:
    Interpreter();

    template <typename StatementContainer>
    void interpret(const StatementContainer& statements)
    {
        try
        {
            for (const auto& statement : statements)
            {
                if (statement)
                {
                    execute(*statement);
                }
            }
        }
        catch (const std::exception& error)
        {
            runtimeError(error);
        }
    }

    void execute(Stmt& statement);

    std::any evaluate(Expr& expression);

    void resolve(
        Expr* expression,
        int depth
    );

private:

    void executeBlock(
        const std::vector<std::shared_ptr<Stmt>>& statements,
        std::shared_ptr<Environment> newEnvironment
    );

    void runtimeError(
        const std::exception& error
    );

    bool isTruthy(const std::any& value) const;

    bool isEqual(
        const std::any& left,
        const std::any& right
    ) const;

    std::string stringify(
        const std::any& value
    ) const;

    bool checkNumberOperand(
        const Token& op,
        const std::any& operand
    ) const;

    bool checkNumberOperands(
        const Token& op,
        const std::any& left,
        const std::any& right
    ) const;

    std::any lookUpVariable(
        const Token& name,
        Expr& expression
    );

    std::any visitAssignExpr(
        Expr::Assign& expr
    ) override;

    std::any visitBinaryExpr(
        Expr::Binary& expr
    ) override;

    std::any visitCallExpr(
        Expr::Call& expr
    ) override;

    std::any visitGetExpr(
        Expr::Get& expr
    ) override;

    std::any visitGroupingExpr(
        Expr::Grouping& expr
    ) override;

    std::any visitLiteralExpr(
        Expr::Literal& expr
    ) override;

    std::any visitLogicalExpr(
        Expr::Logical& expr
    ) override;

    std::any visitSetExpr(
        Expr::Set& expr
    ) override;

    std::any visitSuperExpr(
        Expr::Super& expr
    ) override;

    std::any visitThisExpr(
        Expr::This& expr
    ) override;

    std::any visitUnaryExpr(
        Expr::Unary& expr
    ) override;

    std::any visitVariableExpr(
        Expr::Variable& expr
    ) override;

    std::any visitBlockStmt(
        Stmt::Block& stmt
    ) override;

    std::any visitClassStmt(
        Stmt::Class& stmt
    ) override;

    std::any visitExpressionStmt(
        Stmt::Expression& stmt
    ) override;

    std::any visitFunctionStmt(
        Stmt::Function& stmt
    ) override;

    std::any visitIfStmt(
        Stmt::If& stmt
    ) override;

    std::any visitPrintStmt(
        Stmt::Print& stmt
    ) override;

    std::any visitReturnStmt(
        Stmt::Return& stmt
    ) override;

    std::any visitVarStmt(
        Stmt::Var& stmt
    ) override;

    std::any visitWhileStmt(
        Stmt::While& stmt
    ) override;
};