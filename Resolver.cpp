#include "Resolver.h"
#include "Interpreter.h"

#include <iostream>

Resolver::Resolver(Interpreter& interpreter)
    : interpreter(interpreter)
{
}

// =====================================================
// Resolve program statements
// =====================================================
//
// main.cpp co the goi:
//     resolver.resolve(statements);
//
// Ham nay chuyen tiep sang resolveStatements()
// de tranh xung dot voi resolve(Stmt&) va resolve(Expr&).
//
void Resolver::resolve(
    const std::vector<std::shared_ptr<Stmt>>& statements)
{
    resolveStatements(statements);
}

// =====================================================
// Resolve one statement
// =====================================================

void Resolver::resolve(Stmt& stmt)
{
    stmt.accept(*this);
}

// =====================================================
// Resolve one expression
// =====================================================

void Resolver::resolve(Expr& expr)
{
    expr.accept(*this);
}

// =====================================================
// Scope
// =====================================================

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

// =====================================================
// Variable declaration / definition
// =====================================================

void Resolver::declare(const Token& name)
{
    // Global variable khong can luu trong local scope stack.
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
    }

    // false = declared nhung chua defined.
    scope[name.lexeme] = false;
}

void Resolver::define(const Token& name)
{
    // Global variable khong can luu trong local scope stack.
    if (scopes.empty())
    {
        return;
    }

    // true = da defined.
    scopes.back()[name.lexeme] = true;
}

// =====================================================
// Resolve local variable
// =====================================================

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

    // Khong tim thay trong local scopes
    // => variable la global.
}

// =====================================================
// Resolve function
// =====================================================

void Resolver::resolveFunction(
    const Stmt::Function& function,
    FunctionType type)
{
    // Luu lai function context hien tai.
    const FunctionType enclosingFunction = currentFunction;

    currentFunction = type;

    // Function tao mot local scope moi.
    beginScope();

    // Resolve parameters.
    for (const auto& parameter : function.params)
    {
        declare(parameter);
        define(parameter);
    }

    // Resolve function body.
    resolveStatements(function.body);

    // Ket thuc scope cua function.
    endScope();

    // Khoi phuc context cu.
    currentFunction = enclosingFunction;
}

// =====================================================
// Error
// =====================================================

void Resolver::error(
    const Token& token,
    const std::string& message) const
{
    std::cerr
        << "[line " << token.line << "] Error at '"
        << token.lexeme << "': "
        << message << '\n';
}

// =====================================================
// Expr visitor
// =====================================================

std::any Resolver::visitAssignExpr(Expr::Assign& expr)
{
    // Resolve value truoc.
    resolve(*expr.value);

    // Sau do resolve bien duoc gan.
    resolveLocal(expr, expr.name);

    return {};
}

std::any Resolver::visitBinaryExpr(Expr::Binary& expr)
{
    resolve(*expr.left);
    resolve(*expr.right);

    return {};
}

std::any Resolver::visitCallExpr(Expr::Call& expr)
{
    // Resolve callee.
    resolve(*expr.callee);

    // Resolve tat ca arguments.
    for (const auto& argument : expr.arguments)
    {
        resolve(*argument);
    }

    return {};
}

std::any Resolver::visitGetExpr(Expr::Get& expr)
{
    // Resolve object.
    resolve(*expr.object);

    return {};
}

std::any Resolver::visitGroupingExpr(Expr::Grouping& expr)
{
    resolve(*expr.expression);

    return {};
}

std::any Resolver::visitLiteralExpr(Expr::Literal& /*expr*/)
{
    // Literal khong co variable de resolve.
    return {};
}

std::any Resolver::visitLogicalExpr(Expr::Logical& expr)
{
    resolve(*expr.left);
    resolve(*expr.right);

    return {};
}

std::any Resolver::visitSetExpr(Expr::Set& expr)
{
    // Resolve value truoc.
    resolve(*expr.value);

    // Sau do resolve object.
    resolve(*expr.object);

    return {};
}

std::any Resolver::visitSuperExpr(Expr::Super& expr)
{
    // super chi duoc dung trong class.
    if (currentClass == ClassType::NONE)
    {
        error(
            expr.keyword,
            "Can't use 'super' outside of a class."
        );

        return {};
    }

    // super chi duoc dung trong subclass.
    if (currentClass != ClassType::SUBCLASS)
    {
        error(
            expr.keyword,
            "Can't use 'super' in a class with no superclass."
        );

        return {};
    }

    resolveLocal(expr, expr.keyword);

    return {};
}

std::any Resolver::visitThisExpr(Expr::This& expr)
{
    // this chi duoc dung trong class.
    if (currentClass == ClassType::NONE)
    {
        error(
            expr.keyword,
            "Can't use 'this' outside of a class."
        );

        return {};
    }

    resolveLocal(expr, expr.keyword);

    return {};
}

std::any Resolver::visitUnaryExpr(Expr::Unary& expr)
{
    resolve(*expr.right);

    return {};
}

std::any Resolver::visitVariableExpr(Expr::Variable& expr)
{
    // Neu variable dang duoc declare nhung chua define,
    // khong cho phep doc no trong initializer cua chinh no.
    if (!scopes.empty())
    {
        const auto it =
            scopes.back().find(expr.name.lexeme);

        if (it != scopes.back().end() && !it->second)
        {
            error(
                expr.name,
                "Can't read local variable in its own initializer."
            );
        }
    }

    resolveLocal(expr, expr.name);

    return {};
}

// =====================================================
// Stmt visitor
// =====================================================

void Resolver::visitBlockStmt(Stmt::Block& stmt)
{
    beginScope();

    resolveStatements(stmt.statements);

    endScope();
}

void Resolver::visitClassStmt(Stmt::Class& stmt)
{
    // Luu class context hien tai.
    const ClassType enclosingClass = currentClass;

    currentClass = ClassType::CLASS;

    // Class name phai duoc declare/define truoc.
    declare(stmt.name);
    define(stmt.name);

    // =================================================
    // Superclass
    // =================================================

    if (stmt.superclass)
    {
        // Khong cho:
        // class A < A {}
        if (stmt.name.lexeme == stmt.superclass->name.lexeme)
        {
            error(
                stmt.superclass->name,
                "A class can't inherit from itself."
            );
        }

        currentClass = ClassType::SUBCLASS;

        // Resolve superclass expression.
        resolve(*stmt.superclass);

        // Tao scope danh rieng cho "super".
        beginScope();

        scopes.back()["super"] = true;
    }

    // =================================================
    // this
    // =================================================

    // Tao scope danh rieng cho "this".
    beginScope();

    scopes.back()["this"] = true;

    // =================================================
    // Methods
    // =================================================

    for (const auto& method : stmt.methods)
    {
        FunctionType declaration =
            FunctionType::METHOD;

        if (method.name.lexeme == "init")
        {
            declaration =
                FunctionType::INITIALIZER;
        }

        resolveFunction(method, declaration);
    }

    // Ket thuc scope cua "this".
    endScope();

    // Neu co superclass thi ket thuc scope cua "super".
    if (stmt.superclass)
    {
        endScope();
    }

    // Khoi phuc class context cu.
    currentClass = enclosingClass;
}

void Resolver::visitExpressionStmt(
    Stmt::Expression& stmt)
{
    resolve(*stmt.expression);
}

void Resolver::visitFunctionStmt(
    Stmt::Function& stmt)
{
    // Define function name truoc khi resolve body
    // de ho tro recursion.
    declare(stmt.name);
    define(stmt.name);

    resolveFunction(
        stmt,
        FunctionType::FUNCTION
    );
}

void Resolver::visitIfStmt(
    Stmt::If& stmt)
{
    // Resolve condition.
    resolve(*stmt.condition);

    // Resolve then branch.
    resolve(*stmt.thenBranch);

    // Resolve else branch neu co.
    if (stmt.elseBranch)
    {
        resolve(*stmt.elseBranch);
    }
}

void Resolver::visitPrintStmt(
    Stmt::Print& stmt)
{
    resolve(*stmt.expression);
}

void Resolver::visitReturnStmt(
    Stmt::Return& stmt)
{
    // return khong duoc nam o top-level.
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
        // Initializer khong duoc return value.
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

void Resolver::visitVarStmt(
    Stmt::Var& stmt)
{
    // Declare variable truoc.
    declare(stmt.name);

    // Resolve initializer neu co.
    if (stmt.initializer)
    {
        resolve(*stmt.initializer);
    }

    // Sau khi initializer duoc resolve,
    // variable moi duoc define.
    define(stmt.name);
}

void Resolver::visitWhileStmt(
    Stmt::While& stmt)
{
    resolve(*stmt.condition);
    resolve(*stmt.body);
}