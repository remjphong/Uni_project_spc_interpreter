#pragma once

#include <any>
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "Environment.h"
#include "Token.h"

class Interpreter;
class LoxInstance;
class LoxClass;
class LoxFunction;

class LoxCallable
{
public:
    virtual ~LoxCallable() = default;
    virtual int arity() const = 0;
    virtual std::any call(Interpreter& interpreter,
                          const std::vector<std::any>& arguments) = 0;
    virtual std::string toString() const = 0;
};

class ReturnException : public std::exception
{
private:
    std::any value;

public:
    explicit ReturnException(std::any value);
    const std::any& getValue() const noexcept;
    const char* what() const noexcept override;
};

class LoxFunction : public LoxCallable
{
private:
    const Stmt::Function* declaration;
    std::shared_ptr<Environment> closure;
    bool initializer;

public:
    LoxFunction(const Stmt::Function* declaration,
                std::shared_ptr<Environment> closure,
                bool initializer);

    int arity() const override;
    std::any call(Interpreter& interpreter,
                  const std::vector<std::any>& arguments) override;
    std::string toString() const override;

    std::shared_ptr<LoxFunction> bind(
        const std::shared_ptr<LoxInstance>& instance) const;
};

class LoxClass : public LoxCallable,
                 public std::enable_shared_from_this<LoxClass>
{
private:
    std::string name;
    std::shared_ptr<LoxClass> superclass;
    std::unordered_map<std::string, std::shared_ptr<LoxFunction>> methods;

public:
    LoxClass(std::string name,
             std::shared_ptr<LoxClass> superclass,
             std::unordered_map<std::string, std::shared_ptr<LoxFunction>> methods);

    int arity() const override;
    std::any call(Interpreter& interpreter,
                  const std::vector<std::any>& arguments) override;
    std::string toString() const override;

    std::shared_ptr<LoxFunction> findMethod(
        const std::string& methodName) const;
};

class LoxInstance : public std::enable_shared_from_this<LoxInstance>
{
private:
    std::shared_ptr<LoxClass> klass;
    std::unordered_map<std::string, std::any> fields;

public:
    explicit LoxInstance(std::shared_ptr<LoxClass> klass);

    std::any get(const Token& name);
    void set(const Token& name, const std::any& value);
    std::string toString() const;
};

class Interpreter : public ExprVisitor, public StmtVisitor
{
private:
    std::shared_ptr<Environment> globals;
    std::shared_ptr<Environment> environment;
    std::unordered_map<Expr*, int> locals;

    friend class LoxFunction;

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
                    execute(*statement);
            }
        }
        catch (const std::exception& error)
        {
            runtimeError(error);
        }
    }

    void execute(Stmt& statement);
    std::any evaluate(Expr& expression);
    void resolve(Expr* expression, int depth);

private:
    void executeBlock(const std::vector<std::shared_ptr<Stmt>>& statements,
                      std::shared_ptr<Environment> newEnvironment);
    void runtimeError(const std::exception& error);

    bool isTruthy(const std::any& value) const;
    bool isEqual(const std::any& left, const std::any& right) const;
    std::string stringify(const std::any& value) const;
    bool checkNumberOperand(const Token& op, const std::any& operand) const;
    bool checkNumberOperands(const Token& op,
                             const std::any& left,
                             const std::any& right) const;

    std::any lookUpVariable(const Token& name, Expr& expression);

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