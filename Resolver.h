#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Expr.h"
#include "Stmt.h"
#include "Interpreter.h"

// Resolver thuc hien semantic analysis cho lexical scope.
// File nay gia dinh AST cua ban co cac node:
// Expr::Binary, Grouping, Literal, Unary, Variable, Assign,
// Expr::Logical, Call, Get, Set, This, Super
// va Stmt::Expression, Print, Var, Block, If, While,
// Function, Return, Class.
//
// Interpreter can co ham:
//     void resolve(Expr* expr, int depth);
//
// Neu project cua ban da co FunctionType/ClassType o noi khac,
// co the xoa 2 enum nested ben duoi va dung enum hien tai cua project.

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

    // Scope trong cung nam o scopes.back().
    // value = false: da declare nhung chua define.
    // value = true : da define.
    std::vector<Scope> scopes;

    FunctionType currentFunction = FunctionType::NONE;
    ClassType currentClass = ClassType::NONE;

    bool hadError = false;

public:
    explicit Resolver(Interpreter& interpreter)
        : interpreter(interpreter)
    {
    }

    // Resolve ca chuong trinh.
    template <typename StmtContainer>
    void resolve(const StmtContainer& statements)
    {
        for (const auto& statement : statements)
        {
            if (statement)
            {
                resolve(*statement);
            }
        }
    }

    bool hasError() const
    {
        return hadError;
    }

private:
    // ------------------------------------------------------------
    // Core traversal
    // ------------------------------------------------------------

    void resolve(Stmt& stmt)
    {
        stmt.accept(*this);
    }

    void resolve(Expr& expr)
    {
        expr.accept(*this);
    }

    // ------------------------------------------------------------
    // Scope operations
    // ------------------------------------------------------------

    void beginScope()
    {
        scopes.emplace_back();
    }

    void endScope()
    {
        if (!scopes.empty())
        {
            scopes.pop_back();
        }
    }

    void declare(const Token& name)
    {
        // Bien global khong can luu trong scope stack cua Resolver.
        if (scopes.empty())
        {
            return;
        }

        Scope& scope = scopes.back();

        if (scope.find(name.lexeme) != scope.end())
        {
            error(
                name,
                "Already a variable with this name in this scope."
            );
            return;
        }

        // Da declare, nhung chua define.
        scope[name.lexeme] = false;
    }

    void define(const Token& name)
    {
        // Bien global khong can luu trong scope stack.
        if (scopes.empty())
        {
            return;
        }

        scopes.back()[name.lexeme] = true;
    }

    // Tim declaration tu scope hien tai ra ngoai.
    // Neu tim thay thi ghi lai lexical distance vao Interpreter.
    // Neu khong tim thay thi xem nhu global.
    void resolveLocal(Expr& expr, const Token& name)
    {
        for (int i = static_cast<int>(scopes.size()) - 1; i >= 0; --i)
        {
            if (scopes[i].find(name.lexeme) != scopes[i].end())
            {
                const int distance =
                    static_cast<int>(scopes.size()) - 1 - i;

                interpreter.resolve(&expr, distance);
                return;
            }
        }

        // Khong tim thay trong local scopes => global.
    }

    void resolveFunction(
        const Stmt::Function& function,
        FunctionType type)
    {
        const FunctionType enclosingFunction = currentFunction;
        currentFunction = type;

        beginScope();

        for (const auto& param : function.params)
        {
            declare(param);
            define(param);
        }

        resolve(function.body);

        endScope();
        currentFunction = enclosingFunction;
    }

    void error(const Token& token, const std::string& message)
    {
        hadError = true;

        std::cerr
            << "[Resolver] line "
            << token.line
            << ": "
            << message
            << "\n";
    }

    // ------------------------------------------------------------
    // Expression visitors
    // ------------------------------------------------------------

    void visitAssignExpr(Expr::Assign& expr) override
    {
        // Resolve RHS truoc, sau do resolve bien ben trai.
        resolve(*expr.value);
        resolveLocal(expr, expr.name);
    }

    void visitBinaryExpr(Expr::Binary& expr) override
    {
        resolve(*expr.left);
        resolve(*expr.right);
    }

    void visitCallExpr(Expr::Call& expr) override
    {
        resolve(*expr.callee);

        for (const auto& argument : expr.arguments)
        {
            resolve(*argument);
        }
    }

    void visitGetExpr(Expr::Get& expr) override
    {
        resolve(*expr.object);
    }

    void visitGroupingExpr(Expr::Grouping& expr) override
    {
        resolve(*expr.expression);
    }

    void visitLiteralExpr(Expr::Literal& /*expr*/) override
    {
        // Khong co variable scope can resolve.
    }

    void visitLogicalExpr(Expr::Logical& expr) override
    {
        resolve(*expr.left);
        resolve(*expr.right);
    }

    void visitSetExpr(Expr::Set& expr) override
    {
        // Object va value deu co the chua variable.
        resolve(*expr.value);
        resolve(*expr.object);
    }

    void visitSuperExpr(Expr::Super& expr) override
    {
        if (currentClass == ClassType::NONE)
        {
            error(
                expr.keyword,
                "Can't use 'super' outside of a class."
            );
            return;
        }

        if (currentClass != ClassType::SUBCLASS)
        {
            error(
                expr.keyword,
                "Can't use 'super' in a class with no superclass."
            );
            return;
        }

        resolveLocal(expr, expr.keyword);
    }

    void visitThisExpr(Expr::This& expr) override
    {
        if (currentClass == ClassType::NONE)
        {
            error(
                expr.keyword,
                "Can't use 'this' outside of a class."
            );
            return;
        }

        resolveLocal(expr, expr.keyword);
    }

    void visitUnaryExpr(Expr::Unary& expr) override
    {
        resolve(*expr.right);
    }

    void visitVariableExpr(Expr::Variable& expr) override
    {
        // Bien local da declare nhung chua define:
        // var a = a;
        if (!scopes.empty())
        {
            auto it = scopes.back().find(expr.name.lexeme);

            if (it != scopes.back().end() && !it->second)
            {
                error(
                    expr.name,
                    "Can't read local variable in its own initializer."
                );
            }
        }

        resolveLocal(expr, expr.name);
    }

    // ------------------------------------------------------------
    // Statement visitors
    // ------------------------------------------------------------

    void visitBlockStmt(Stmt::Block& stmt) override
    {
        beginScope();
        resolve(stmt.statements);
        endScope();
    }

    void visitClassStmt(Stmt::Class& stmt) override
    {
        const ClassType enclosingClass = currentClass;
        currentClass = ClassType::CLASS;

        // Ten class vao scope hien tai de co the tham chieu class bang ten.
        declare(stmt.name);
        define(stmt.name);

        // Xu ly superclass.
        if (stmt.superclass)
        {
            // class A < A {}
            if (stmt.name.lexeme == stmt.superclass->name.lexeme)
            {
                error(
                    stmt.superclass->name,
                    "A class can't inherit from itself."
                );
            }

            currentClass = ClassType::SUBCLASS;

            resolve(*stmt.superclass);

            // super nam trong scope rieng de method co the resolve.
            beginScope();
            scopes.back()["super"] = true;
        }

        // this nam trong scope ngay ben ngoai method body.
        beginScope();
        scopes.back()["this"] = true;

        for (const auto& method : stmt.methods)
        {
            FunctionType declaration = FunctionType::METHOD;

            // Lox dung init lam constructor.
            if (method.name.lexeme == "init")
            {
                declaration = FunctionType::INITIALIZER;
            }

            // Neu methods la vector object (khong phai pointer)
            // thi dong nay dung truc tiep.
            resolveFunction(method, declaration);
        }

        // Dong scope cua this.
        endScope();

        // Neu la subclass thi dong scope cua super.
        if (stmt.superclass)
        {
            endScope();
        }

        currentClass = enclosingClass;
    }

    void visitExpressionStmt(Stmt::Expression& stmt) override
    {
        resolve(*stmt.expression);
    }

    void visitFunctionStmt(Stmt::Function& stmt) override
    {
        // Declare/define function truoc khi resolve body de ho tro recursion.
        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt, FunctionType::FUNCTION);
    }

    void visitIfStmt(Stmt::If& stmt) override
    {
        resolve(*stmt.condition);
        resolve(*stmt.thenBranch);

        if (stmt.elseBranch)
        {
            resolve(*stmt.elseBranch);
        }
    }

    void visitPrintStmt(Stmt::Print& stmt) override
    {
        resolve(*stmt.expression);
    }

    void visitReturnStmt(Stmt::Return& stmt) override
    {
        if (currentFunction == FunctionType::NONE)
        {
            error(
                stmt.keyword,
                "Can't return from top-level code."
            );
        }

        if (!stmt.value)
        {
            return;
        }

        if (currentFunction == FunctionType::INITIALIZER)
        {
            error(
                stmt.keyword,
                "Can't return a value from an initializer."
            );
        }

        resolve(*stmt.value);
    }

    void visitVarStmt(Stmt::Var& stmt) override
    {
        // Thu tu nay bat buoc de bat var a = a;
        declare(stmt.name);

        if (stmt.initializer)
        {
            resolve(*stmt.initializer);
        }

        define(stmt.name);
    }

    void visitWhileStmt(Stmt::While& stmt) override
    {
        resolve(*stmt.condition);
        resolve(*stmt.body);
    }
};
