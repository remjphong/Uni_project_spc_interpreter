#include "Interpreter.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

Interpreter::Interpreter()
{
    globals = std::make_shared<Environment>();
    environment = globals;
}

void Interpreter::execute(Stmt& statement)
{
    statement.accept(*this);
}

std::any Interpreter::evaluate(Expr& expression)
{
    return expression.accept(*this);
}

void Interpreter::resolve(
    Expr* expression,
    int depth
)
{
    locals[expression] = depth;
}

void Interpreter::executeBlock(
    const std::vector<std::shared_ptr<Stmt>>& statements,
    std::shared_ptr<Environment> newEnvironment
)
{
    std::shared_ptr<Environment> previous = environment;

    try
    {
        environment = newEnvironment;

        for (const auto& statement : statements)
        {
            if (statement)
            {
                execute(*statement);
            }
        }
    }
    catch (...)
    {
        environment = previous;
        throw;
    }

    environment = previous;
}

void Interpreter::runtimeError(
    const std::exception& error
)
{
    std::cerr << "Runtime error: "
              << error.what()
              << '\n';
}

bool Interpreter::isTruthy(
    const std::any& value
) const
{
    if (!value.has_value())
    {
        return false;
    }

    if (value.type() == typeid(bool))
    {
        return std::any_cast<bool>(value);
    }

    return true;
}

bool Interpreter::isEqual(
    const std::any& left,
    const std::any& right
) const
{
    if (!left.has_value() && !right.has_value())
    {
        return true;
    }

    if (!left.has_value() || !right.has_value())
    {
        return false;
    }

    if (left.type() != right.type())
    {
        return false;
    }

    if (left.type() == typeid(double))
    {
        return std::any_cast<double>(left)
            == std::any_cast<double>(right);
    }

    if (left.type() == typeid(bool))
    {
        return std::any_cast<bool>(left)
            == std::any_cast<bool>(right);
    }

    if (left.type() == typeid(std::string))
    {
        return std::any_cast<std::string>(left)
            == std::any_cast<std::string>(right);
    }

    return false;
}

std::string Interpreter::stringify(
    const std::any& value
) const
{
    if (!value.has_value())
    {
        return "nil";
    }

    if (value.type() == typeid(double))
    {
        double number = std::any_cast<double>(value);

        if (std::floor(number) == number)
        {
            return std::to_string(
                static_cast<long long>(number)
            );
        }

        return std::to_string(number);
    }

    if (value.type() == typeid(bool))
    {
        return std::any_cast<bool>(value)
            ? "true"
            : "false";
    }

    if (value.type() == typeid(std::string))
    {
        return std::any_cast<std::string>(value);
    }

    return "<object>";
}

bool Interpreter::checkNumberOperand(
    const Token& op,
    const std::any& operand
) const
{
    if (operand.type() == typeid(double))
    {
        return true;
    }

    throw std::runtime_error(
        "Operand must be a number."
    );
}

bool Interpreter::checkNumberOperands(
    const Token& op,
    const std::any& left,
    const std::any& right
) const
{
    if (left.type() == typeid(double) &&
        right.type() == typeid(double))
    {
        return true;
    }

    throw std::runtime_error(
        "Operands must be numbers."
    );
}

std::any Interpreter::lookUpVariable(
    const Token& name,
    Expr& expression
)
{
    auto it = locals.find(&expression);

    if (it != locals.end())
    {
        int distance = it->second;

        return environment->getAt(
            distance,
            name.lexeme
        );
    }

    return globals->get(name.lexeme);
}


std::any Interpreter::visitLiteralExpr(
    Expr::Literal& expr
)
{
    return expr.value;
}

std::any Interpreter::visitGroupingExpr(
    Expr::Grouping& expr
)
{
    return evaluate(*expr.expression);
}

std::any Interpreter::visitUnaryExpr(
    Expr::Unary& expr
)
{
    std::any right = evaluate(*expr.right);

    switch (expr.op.type)
    {
        case TokenType::MINUS:
        {
            checkNumberOperand(
                expr.op,
                right
            );

            return -std::any_cast<double>(right);
        }

        case TokenType::BANG:
            return !isTruthy(right);

        default:
            break;
    }

    return {};
}

std::any Interpreter::visitBinaryExpr(
    Expr::Binary& expr
)
{
    std::any left = evaluate(*expr.left);
    std::any right = evaluate(*expr.right);

    switch (expr.op.type)
    {
        case TokenType::MINUS:
            checkNumberOperands(
                expr.op,
                left,
                right
            );

            return std::any_cast<double>(left)
                - std::any_cast<double>(right);

        case TokenType::SLASH:
            checkNumberOperands(
                expr.op,
                left,
                right
            );

            return std::any_cast<double>(left)
                / std::any_cast<double>(right);

        case TokenType::STAR:
            checkNumberOperands(
                expr.op,
                left,
                right
            );

            return std::any_cast<double>(left)
                * std::any_cast<double>(right);

        case TokenType::PLUS:
        {
            if (left.type() == typeid(double) &&
                right.type() == typeid(double))
            {
                return std::any_cast<double>(left)
                    + std::any_cast<double>(right);
            }

            if (left.type() == typeid(std::string) &&
                right.type() == typeid(std::string))
            {
                return std::any_cast<std::string>(left)
                    + std::any_cast<std::string>(right);
            }

            throw std::runtime_error(
                "Operands must be two numbers or two strings."
            );
        }

        case TokenType::GREATER:
            checkNumberOperands(expr.op, left, right);

            return std::any_cast<double>(left)
                > std::any_cast<double>(right);

        case TokenType::GREATER_EQUAL:
            checkNumberOperands(expr.op, left, right);

            return std::any_cast<double>(left)
                >= std::any_cast<double>(right);

        case TokenType::LESS:
            checkNumberOperands(expr.op, left, right);

            return std::any_cast<double>(left)
                < std::any_cast<double>(right);

        case TokenType::LESS_EQUAL:
            checkNumberOperands(expr.op, left, right);

            return std::any_cast<double>(left)
                <= std::any_cast<double>(right);

        case TokenType::BANG_EQUAL:
            return !isEqual(left, right);

        case TokenType::EQUAL_EQUAL:
            return isEqual(left, right);

        default:
            break;
    }

    return {};
}

std::any Interpreter::visitVariableExpr(
    Expr::Variable& expr
)
{
    return lookUpVariable(
        expr.name,
        expr
    );
}

std::any Interpreter::visitAssignExpr(
    Expr::Assign& expr
)
{
    std::any value = evaluate(*expr.value);

    auto it = locals.find(&expr);

    if (it != locals.end())
    {
        environment->assignAt(
            it->second,
            expr.name.lexeme,
            value
        );
    }
    else
    {
        globals->assign(
            expr.name.lexeme,
            value
        );
    }

    return value;
}

std::any Interpreter::visitLogicalExpr(
    Expr::Logical& expr
)
{
    std::any left = evaluate(*expr.left);

    if (expr.op.type == TokenType::OR)
    {
        if (isTruthy(left))
        {
            return left;
        }
    }
    else
    {
        if (!isTruthy(left))
        {
            return left;
        }
    }

    return evaluate(*expr.right);
}

std::any Interpreter::visitCallExpr(
    Expr::Call& expr
)
{
    std::any callee = evaluate(*expr.callee);

    std::vector<std::any> arguments;

    for (const auto& argument : expr.arguments)
    {
        arguments.push_back(
            evaluate(*argument)
        );
    }

    if (callee.type() != typeid(
            std::shared_ptr<LoxCallable>))
    {
        throw std::runtime_error(
            "Can only call functions and classes."
        );
    }

    auto function =
        std::any_cast<std::shared_ptr<LoxCallable>>(
            callee
        );

    if (arguments.size() !=
        static_cast<size_t>(function->arity()))
    {
        throw std::runtime_error(
            "Expected " +
            std::to_string(function->arity()) +
            " arguments but got " +
            std::to_string(arguments.size()) +
            "."
        );
    }

    return function->call(
        *this,
        arguments
    );
}

std::any Interpreter::visitGetExpr(
    Expr::Get& expr
)
{
    // Class instance handling sẽ phụ thuộc
    // vào runtime object của AST/project.
    return {};
}

std::any Interpreter::visitSetExpr(
    Expr::Set& expr
)
{
    return {};
}

std::any Interpreter::visitThisExpr(
    Expr::This& expr
)
{
    return lookUpVariable(
        expr.keyword,
        expr
    );
}

std::any Interpreter::visitSuperExpr(
    Expr::Super& expr
)
{
    return {};
}


// =========================
// Statement visitor
// =========================

std::any Interpreter::visitExpressionStmt(
    Stmt::Expression& stmt
)
{
    evaluate(*stmt.expression);

    return {};
}

std::any Interpreter::visitPrintStmt(
    Stmt::Print& stmt
)
{
    std::any value =
        evaluate(*stmt.expression);

    std::cout << stringify(value)
              << '\n';

    return {};
}

std::any Interpreter::visitVarStmt(
    Stmt::Var& stmt
)
{
    std::any value;

    if (stmt.initializer)
    {
        value =
            evaluate(*stmt.initializer);
    }

    environment->define(
        stmt.name.lexeme,
        value
    );

    return {};
}

std::any Interpreter::visitBlockStmt(
    Stmt::Block& stmt
)
{
    auto blockEnvironment =
        std::make_shared<Environment>(
            environment
        );

    executeBlock(
        stmt.statements,
        blockEnvironment
    );

    return {};
}

std::any Interpreter::visitIfStmt(
    Stmt::If& stmt
)
{
    if (isTruthy(
        evaluate(*stmt.condition)
    ))
    {
        execute(*stmt.thenBranch);
    }
    else if (stmt.elseBranch)
    {
        execute(*stmt.elseBranch);
    }

    return {};
}

std::any Interpreter::visitWhileStmt(
    Stmt::While& stmt
)
{
    while (isTruthy(
        evaluate(*stmt.condition)
    ))
    {
        execute(*stmt.body);
    }

    return {};
}

std::any Interpreter::visitFunctionStmt(
    Stmt::Function& stmt
)
{
    return {};
}

std::any Interpreter::visitReturnStmt(
    Stmt::Return& stmt
)
{
    return {};
}

std::any Interpreter::visitClassStmt(
    Stmt::Class& stmt
)
{
    return {};
}