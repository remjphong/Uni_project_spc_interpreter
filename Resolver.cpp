#include "Resolver.h"
#include "Interpreter.h"

#include <iostream>

Resolver::Resolver(Interpreter& interpreter)
    : interpreter(interpreter)
{
}

void Resolver::resolve(Stmt& stmt)
{
    stmt.accept(*this);
}

void Resolver::resolve(Expr& expr)
{
    expr.accept(*this);
}

void Resolver::beginScope()
{
    scopes.emplace_back();
}

void Resolver::endScope()
{
    if (!scopes.empty())
    {
        scopes.pop_back();
    }
}

void Resolver::declare(const Token& name)
{
    if (scopes.empty())
    {
        return; // Global khong can luu trong scope stack.
    }

    Scope& scope = scopes.back();

    if (scope.find(name.lexeme) != scope.end())
    {
        error(
            name,
            "Already a variable with this name in this scope."
        );
    }

    // false = da declare, chua define.
    scope[name.lexeme] = false;
}

void Resolver::define(const Token& name)
{
    if (scopes.empty())
    {
        return;
    }

    scopes.back()[name.lexeme] = true;
}

void Resolver::resolveLocal(Expr& expr, const Token& name)
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

    // Khong tim thay trong local scopes => bien global.
}

void Resolver::resolveFunction(
    const Stmt::Function& function,
    FunctionType type)
{
    const FunctionType enclosingFunction = currentFunction;
    currentFunction = type;

    beginScope();

    for (const auto& parameter : function.params)
    {
        declare(parameter);
        define(parameter);
    }

    resolve(function.body);

    endScope();

    currentFunction = enclosingFunction;
}

void Resolver::error(
    const Token& token,
    const std::string& message) const
{
    std::cerr
        << "[line " << token.line << "] Error at '"
        << token.lexeme << "': "
        << message << '\n';
}

// =========================
// Expr visitor
// =========================

void Resolver::visitAssignExpr(Expr::Assign& expr)
{
    resolve(*expr.value);
    resolveLocal(expr, expr.name);
}

void Resolver::visitBinaryExpr(Expr::Binary& expr)
{
    resolve(*expr.left);
    resolve(*expr.right);
}

void Resolver::visitCallExpr(Expr::Call& expr)
{
    resolve(*expr.callee);

    for (const auto& argument : expr.arguments)
    {
        resolve(*argument);
    }
}

void Resolver::visitGetExpr(Expr::Get& expr)
{
    resolve(*expr.object);
}

void Resolver::visitGroupingExpr(Expr::Grouping& expr)
{
    resolve(*expr.expression);
}

void Resolver::visitLiteralExpr(Expr::Literal& /*expr*/)
{
    // Khong co gi de resolve.
}

void Resolver::visitLogicalExpr(Expr::Logical& expr)
{
    resolve(*expr.left);
    resolve(*expr.right);
}

void Resolver::visitSetExpr(Expr::Set& expr)
{
    // Resolve value truoc, sau do resolve object.
    resolve(*expr.value);
    resolve(*expr.object);
}

void Resolver::visitSuperExpr(Expr::Super& expr)
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

void Resolver::visitThisExpr(Expr::This& expr)
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

void Resolver::visitUnaryExpr(Expr::Unary& expr)
{
    resolve(*expr.right);
}

void Resolver::visitVariableExpr(Expr::Variable& expr)
{
    if (!scopes.empty())
    {
        const auto it = scopes.back().find(expr.name.lexeme);

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

// =========================
// Stmt visitor
// =========================

void Resolver::visitBlockStmt(Stmt::Block& stmt)
{
    beginScope();
    resolve(stmt.statements);
    endScope();
}

void Resolver::visitClassStmt(Stmt::Class& stmt)
{
    const ClassType enclosingClass = currentClass;
    currentClass = ClassType::CLASS;

    declare(stmt.name);
    define(stmt.name);

    if (stmt.superclass)
    {
        // class A < A {} la loi.
        if (stmt.name.lexeme == stmt.superclass->name.lexeme)
        {
            error(
                stmt.superclass->name,
                "A class can't inherit from itself."
            );
        }

        currentClass = ClassType::SUBCLASS;
        resolve(*stmt.superclass);

        // Scope nay chua 'super'.
        beginScope();
        scopes.back()["super"] = true;
    }

    // Scope nay chua 'this'.
    beginScope();
    scopes.back()["this"] = true;

    for (const auto& method : stmt.methods)
    {
        FunctionType declaration = FunctionType::METHOD;

        if (method.name.lexeme == "init")
        {
            declaration = FunctionType::INITIALIZER;
        }

        resolveFunction(method, declaration);
    }

    endScope(); // 'this'

    if (stmt.superclass)
    {
        endScope(); // 'super'
    }

    currentClass = enclosingClass;
}

void Resolver::visitExpressionStmt(Stmt::Expression& stmt)
{
    resolve(*stmt.expression);
}

void Resolver::visitFunctionStmt(Stmt::Function& stmt)
{
    // Define ten ham truoc khi resolve body de ho tro recursion.
    declare(stmt.name);
    define(stmt.name);

    resolveFunction(stmt, FunctionType::FUNCTION);
}

void Resolver::visitIfStmt(Stmt::If& stmt)
{
    resolve(*stmt.condition);
    resolve(*stmt.thenBranch);

    if (stmt.elseBranch)
    {
        resolve(*stmt.elseBranch);
    }
}

void Resolver::visitPrintStmt(Stmt::Print& stmt)
{
    resolve(*stmt.expression);
}

void Resolver::visitReturnStmt(Stmt::Return& stmt)
{
    if (currentFunction == FunctionType::NONE)
    {
        error(
            stmt.keyword,
            "Can't return from top-level code."
        );
        return;
    }

    if (stmt.value)
    {
        if (currentFunction == FunctionType::INITIALIZER)
        {
            error(
                stmt.keyword,
                "Can't return a value from an initializer."
            );
        }

        resolve(*stmt.value);
    }
}

void Resolver::visitVarStmt(Stmt::Var& stmt)
{
    declare(stmt.name);

    if (stmt.initializer)
    {
        resolve(*stmt.initializer);
    }

    define(stmt.name);
}

void Resolver::visitWhileStmt(Stmt::While& stmt)
{
    resolve(*stmt.condition);
    resolve(*stmt.body);
}
