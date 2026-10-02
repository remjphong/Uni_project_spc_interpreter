#include "Interpreter.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

ReturnException::ReturnException(std::any value)
    : value(std::move(value)) {}

const std::any& ReturnException::getValue() const noexcept
{
    return value;
}

const char* ReturnException::what() const noexcept
{
    return "Lox return";
}

LoxFunction::LoxFunction(const Stmt::Function* declaration,
                         std::shared_ptr<Environment> closure,
                         bool initializer)
    : declaration(declaration),
      closure(std::move(closure)),
      initializer(initializer) {}

int LoxFunction::arity() const
{
    return static_cast<int>(declaration->params.size());
}

std::any LoxFunction::call(Interpreter& interpreter,
                          const std::vector<std::any>& arguments)
{
    auto functionEnvironment =
        std::make_shared<Environment>(closure);

    for (std::size_t i = 0; i < declaration->params.size(); ++i)
    {
        functionEnvironment->define(
            declaration->params[i].lexeme,
            arguments[i]
        );
    }

    try
    {
        interpreter.executeBlock(
            declaration->body,
            functionEnvironment
        );
    }
    catch (const ReturnException& returnValue)
    {
        if (initializer)
            return closure->get("this");

        return returnValue.getValue();
    }

    if (initializer)
        return closure->get("this");

    return {};
}

std::string LoxFunction::toString() const
{
    return "<fn " + declaration->name.lexeme + ">";
}

std::shared_ptr<LoxFunction> LoxFunction::bind(
    const std::shared_ptr<LoxInstance>& instance) const
{
    auto boundEnvironment =
        std::make_shared<Environment>(closure);

    boundEnvironment->define("this", instance);

    return std::make_shared<LoxFunction>(
        declaration,
        boundEnvironment,
        initializer
    );
}

LoxClass::LoxClass(
    std::string name,
    std::shared_ptr<LoxClass> superclass,
    std::unordered_map<std::string, std::shared_ptr<LoxFunction>> methods)
    : name(std::move(name)),
      superclass(std::move(superclass)),
      methods(std::move(methods)) {}

int LoxClass::arity() const
{
    auto initializer = findMethod("init");
    return initializer ? initializer->arity() : 0;
}

std::any LoxClass::call(
    Interpreter& interpreter,
    const std::vector<std::any>& arguments)
{
    auto instance =
        std::make_shared<LoxInstance>(shared_from_this());

    auto initializer = findMethod("init");
    if (initializer)
    {
        initializer->bind(instance)->call(
            interpreter,
            arguments
        );
    }

    return instance;
}

std::string LoxClass::toString() const
{
    return name;
}

std::shared_ptr<LoxFunction> LoxClass::findMethod(
    const std::string& methodName) const
{
    auto it = methods.find(methodName);
    if (it != methods.end())
        return it->second;

    if (superclass)
        return superclass->findMethod(methodName);

    return nullptr;
}

LoxInstance::LoxInstance(std::shared_ptr<LoxClass> klass)
    : klass(std::move(klass)) {}

std::any LoxInstance::get(const Token& name)
{
    auto field = fields.find(name.lexeme);
    if (field != fields.end())
        return field->second;

    auto method = klass->findMethod(name.lexeme);
    if (method)
    {
        auto bound = method->bind(shared_from_this());
        return std::static_pointer_cast<LoxCallable>(bound);
    }

    throw std::runtime_error(
        "Undefined property '" + name.lexeme + "'."
    );
}

void LoxInstance::set(const Token& name, const std::any& value)
{
    fields[name.lexeme] = value;
}

std::string LoxInstance::toString() const
{
    return "<" + klass->toString() + " instance>";
}

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

void Interpreter::resolve(Expr* expression, int depth)
{
    locals[expression] = depth;
}

void Interpreter::executeBlock(
    const std::vector<std::shared_ptr<Stmt>>& statements,
    std::shared_ptr<Environment> newEnvironment)
{
    auto previous = environment;

    try
    {
        environment = std::move(newEnvironment);

        for (const auto& statement : statements)
        {
            if (statement)
                execute(*statement);
        }
    }
    catch (...)
    {
        environment = previous;
        throw;
    }

    environment = previous;
}

void Interpreter::runtimeError(const std::exception& error)
{
    std::cerr << "Runtime error: " << error.what() << '\n';
}

bool Interpreter::isTruthy(const std::any& value) const
{
    if (!value.has_value())
        return false;

    if (value.type() == typeid(bool))
        return std::any_cast<bool>(value);

    return true;
}

bool Interpreter::isEqual(const std::any& left,
                          const std::any& right) const
{
    if (!left.has_value() && !right.has_value())
        return true;

    if (!left.has_value() || !right.has_value())
        return false;

    if (left.type() != right.type())
        return false;

    if (left.type() == typeid(double))
        return std::any_cast<double>(left) == std::any_cast<double>(right);

    if (left.type() == typeid(bool))
        return std::any_cast<bool>(left) == std::any_cast<bool>(right);

    if (left.type() == typeid(std::string))
        return std::any_cast<std::string>(left) == std::any_cast<std::string>(right);

    if (left.type() == typeid(std::shared_ptr<LoxCallable>))
    {
        return std::any_cast<std::shared_ptr<LoxCallable>>(left) ==
               std::any_cast<std::shared_ptr<LoxCallable>>(right);
    }

    if (left.type() == typeid(std::shared_ptr<LoxInstance>))
    {
        return std::any_cast<std::shared_ptr<LoxInstance>>(left) ==
               std::any_cast<std::shared_ptr<LoxInstance>>(right);
    }

    return false;
}

std::string Interpreter::stringify(const std::any& value) const
{
    if (!value.has_value())
        return "nil";

    if (value.type() == typeid(double))
    {
        double number = std::any_cast<double>(value);
        if (std::floor(number) == number)
        {
            return std::to_string(
                static_cast<long long>(number)
            );
        }

        std::string result = std::to_string(number);
        while (!result.empty() && result.back() == '0')
            result.pop_back();
        if (!result.empty() && result.back() == '.')
            result.pop_back();
        return result;
    }

    if (value.type() == typeid(bool))
        return std::any_cast<bool>(value) ? "true" : "false";

    if (value.type() == typeid(std::string))
        return std::any_cast<std::string>(value);

    if (value.type() == typeid(std::shared_ptr<LoxCallable>))
    {
        auto callable = std::any_cast<std::shared_ptr<LoxCallable>>(value);
        return callable ? callable->toString() : "nil";
    }

    if (value.type() == typeid(std::shared_ptr<LoxInstance>))
    {
        auto instance = std::any_cast<std::shared_ptr<LoxInstance>>(value);
        return instance ? instance->toString() : "nil";
    }

    return "<object>";
}

bool Interpreter::checkNumberOperand(const Token&, const std::any& operand) const
{
    if (operand.type() == typeid(double))
        return true;

    throw std::runtime_error("Operand must be a number.");
}

bool Interpreter::checkNumberOperands(const Token&,
                                      const std::any& left,
                                      const std::any& right) const
{
    if (left.type() == typeid(double) &&
        right.type() == typeid(double))
        return true;

    throw std::runtime_error("Operands must be numbers.");
}

std::any Interpreter::lookUpVariable(const Token& name,
                                     Expr& expression)
{
    auto it = locals.find(&expression);

    if (it != locals.end())
    {
        return environment->getAt(
            it->second,
            name.lexeme
        );
    }

    return globals->get(name.lexeme);
}

std::any Interpreter::visitLiteralExpr(Expr::Literal& expr)
{
    return expr.value;
}

std::any Interpreter::visitGroupingExpr(Expr::Grouping& expr)
{
    return evaluate(*expr.expression);
}

std::any Interpreter::visitUnaryExpr(Expr::Unary& expr)
{
    auto right = evaluate(*expr.right);

    switch (expr.op.type)
    {
        case TokenType::MINUS:
            checkNumberOperand(expr.op, right);
            return -std::any_cast<double>(right);

        case TokenType::BANG:
            return !isTruthy(right);

        default:
            return {};
    }
}

std::any Interpreter::visitBinaryExpr(Expr::Binary& expr)
{
    auto left = evaluate(*expr.left);
    auto right = evaluate(*expr.right);

    switch (expr.op.type)
    {
        case TokenType::MINUS:
            checkNumberOperands(expr.op, left, right);
            return std::any_cast<double>(left) - std::any_cast<double>(right);

        case TokenType::SLASH:
            checkNumberOperands(expr.op, left, right);
            return std::any_cast<double>(left) / std::any_cast<double>(right);

        case TokenType::STAR:
            checkNumberOperands(expr.op, left, right);
            return std::any_cast<double>(left) * std::any_cast<double>(right);

        case TokenType::PLUS:
            if (left.type() == typeid(double) && right.type() == typeid(double))
                return std::any_cast<double>(left) + std::any_cast<double>(right);

            if (left.type() == typeid(std::string) && right.type() == typeid(std::string))
                return std::any_cast<std::string>(left) + std::any_cast<std::string>(right);

            throw std::runtime_error(
                "Operands must be two numbers or two strings."
            );

        case TokenType::GREATER:
            checkNumberOperands(expr.op, left, right);
            return std::any_cast<double>(left) > std::any_cast<double>(right);

        case TokenType::GREATER_EQUAL:
            checkNumberOperands(expr.op, left, right);
            return std::any_cast<double>(left) >= std::any_cast<double>(right);

        case TokenType::LESS:
            checkNumberOperands(expr.op, left, right);
            return std::any_cast<double>(left) < std::any_cast<double>(right);

        case TokenType::LESS_EQUAL:
            checkNumberOperands(expr.op, left, right);
            return std::any_cast<double>(left) <= std::any_cast<double>(right);

        case TokenType::BANG_EQUAL:
            return !isEqual(left, right);

        case TokenType::EQUAL_EQUAL:
            return isEqual(left, right);

        default:
            return {};
    }
}

std::any Interpreter::visitVariableExpr(Expr::Variable& expr)
{
    return lookUpVariable(expr.name, expr);
}

std::any Interpreter::visitAssignExpr(Expr::Assign& expr)
{
    auto value = evaluate(*expr.value);
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
        globals->assign(expr.name.lexeme, value);
    }

    return value;
}

std::any Interpreter::visitLogicalExpr(Expr::Logical& expr)
{
    auto left = evaluate(*expr.left);

    if (expr.op.type == TokenType::OR)
    {
        if (isTruthy(left))
            return left;
    }
    else if (expr.op.type == TokenType::AND)
    {
        if (!isTruthy(left))
            return left;
    }

    return evaluate(*expr.right);
}

std::any Interpreter::visitCallExpr(Expr::Call& expr)
{
    auto callee = evaluate(*expr.callee);

    std::vector<std::any> arguments;
    arguments.reserve(expr.arguments.size());

    for (const auto& argument : expr.arguments)
        arguments.push_back(evaluate(*argument));

    if (callee.type() != typeid(std::shared_ptr<LoxCallable>))
    {
        throw std::runtime_error(
            "Can only call functions and classes."
        );
    }

    auto function =
        std::any_cast<std::shared_ptr<LoxCallable>>(callee);

    if (!function)
        throw std::runtime_error("Can only call functions and classes.");

    if (arguments.size() != static_cast<std::size_t>(function->arity()))
    {
        throw std::runtime_error(
            "Expected " + std::to_string(function->arity()) +
            " arguments but got " + std::to_string(arguments.size()) + "."
        );
    }

    return function->call(*this, arguments);
}

std::any Interpreter::visitGetExpr(Expr::Get& expr)
{
    auto object = evaluate(*expr.object);

    if (object.type() != typeid(std::shared_ptr<LoxInstance>))
    {
        throw std::runtime_error(
            "Only instances have properties."
        );
    }

    auto instance =
        std::any_cast<std::shared_ptr<LoxInstance>>(object);

    return instance->get(expr.name);
}

std::any Interpreter::visitSetExpr(Expr::Set& expr)
{
    auto object = evaluate(*expr.object);

    if (object.type() != typeid(std::shared_ptr<LoxInstance>))
    {
        throw std::runtime_error(
            "Only instances have fields."
        );
    }

    auto instance =
        std::any_cast<std::shared_ptr<LoxInstance>>(object);

    auto value = evaluate(*expr.value);
    instance->set(expr.name, value);
    return value;
}

std::any Interpreter::visitThisExpr(Expr::This& expr)
{
    return lookUpVariable(expr.keyword, expr);
}

std::any Interpreter::visitSuperExpr(Expr::Super& expr)
{
    auto local = locals.find(&expr);
    if (local == locals.end())
        throw std::runtime_error("Unable to resolve 'super'.");

    int distance = local->second;

    auto superclassValue =
        environment->getAt(distance, "super");

    if (superclassValue.type() != typeid(std::shared_ptr<LoxClass>))
        throw std::runtime_error("Invalid superclass.");

    auto superclass =
        std::any_cast<std::shared_ptr<LoxClass>>(superclassValue);

    auto thisValue =
        environment->getAt(distance - 1, "this");

    if (thisValue.type() != typeid(std::shared_ptr<LoxInstance>))
        throw std::runtime_error("Invalid 'this' reference.");

    auto instance =
        std::any_cast<std::shared_ptr<LoxInstance>>(thisValue);

    auto method = superclass->findMethod(expr.method.lexeme);
    if (!method)
    {
        throw std::runtime_error(
            "Undefined property '" + expr.method.lexeme + "'."
        );
    }

    return std::static_pointer_cast<LoxCallable>(
        method->bind(instance)
    );
}

void Interpreter::visitExpressionStmt(Stmt::Expression& stmt)
{
    evaluate(*stmt.expression);
}

void Interpreter::visitPrintStmt(Stmt::Print& stmt)
{
    std::cout << stringify(evaluate(*stmt.expression)) << '\n';
}

void Interpreter::visitVarStmt(Stmt::Var& stmt)
{
    std::any value;

    if (stmt.initializer)
        value = evaluate(*stmt.initializer);

    environment->define(stmt.name.lexeme, value);
}

void Interpreter::visitBlockStmt(Stmt::Block& stmt)
{
    auto blockEnvironment =
        std::make_shared<Environment>(environment);

    executeBlock(stmt.statements, blockEnvironment);
}

void Interpreter::visitIfStmt(Stmt::If& stmt)
{
    if (isTruthy(evaluate(*stmt.condition)))
    {
        if (stmt.thenBranch)
            execute(*stmt.thenBranch);
    }
    else if (stmt.elseBranch)
    {
        execute(*stmt.elseBranch);
    }
}

void Interpreter::visitWhileStmt(Stmt::While& stmt)
{
    while (isTruthy(evaluate(*stmt.condition)))
        execute(*stmt.body);
}

void Interpreter::visitFunctionStmt(Stmt::Function& stmt)
{
    auto function = std::make_shared<LoxFunction>(
        &stmt,
        environment,
        false
    );

    std::shared_ptr<LoxCallable> callable = function;
    environment->define(stmt.name.lexeme, callable);
}

void Interpreter::visitReturnStmt(Stmt::Return& stmt)
{
    std::any value;

    if (stmt.value)
        value = evaluate(*stmt.value);

    throw ReturnException(std::move(value));
}

void Interpreter::visitClassStmt(Stmt::Class& stmt)
{
    std::shared_ptr<LoxClass> superclass;

    if (stmt.superclass)
    {
        auto value = evaluate(*stmt.superclass);

        if (value.type() == typeid(std::shared_ptr<LoxClass>))
        {
            superclass =
                std::any_cast<std::shared_ptr<LoxClass>>(value);
        }
        else if (value.type() == typeid(std::shared_ptr<LoxCallable>))
        {
            auto callable =
                std::any_cast<std::shared_ptr<LoxCallable>>(value);

            superclass =
                std::dynamic_pointer_cast<LoxClass>(callable);

            if (!superclass)
                throw std::runtime_error("Superclass must be a class.");
        }
        else
        {
            throw std::runtime_error("Superclass must be a class.");
        }
    }

    environment->define(stmt.name.lexeme, {});

    auto previous = environment;

    if (superclass)
    {
        environment =
            std::make_shared<Environment>(environment);

        environment->define("super", superclass);
    }

    std::unordered_map<
        std::string,
        std::shared_ptr<LoxFunction>
    > methods;

    for (auto& method : stmt.methods)
    {
        bool initializer = method.name.lexeme == "init";

        methods[method.name.lexeme] =
            std::make_shared<LoxFunction>(
                &method,
                environment,
                initializer
            );
    }

    auto klass = std::make_shared<LoxClass>(
        stmt.name.lexeme,
        superclass,
        std::move(methods)
    );

    if (superclass)
        environment = previous;

    std::shared_ptr<LoxCallable> callable = klass;
    environment->assign(stmt.name.lexeme, callable);
}
