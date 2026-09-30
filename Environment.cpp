#include "Environment.h"

#include <stdexcept>
#include <utility>

Environment::Environment()
    : enclosing(nullptr)
{
}

Environment::Environment(
    std::shared_ptr<Environment> enclosing
)
    : enclosing(std::move(enclosing))
{
}

void Environment::define(
    const std::string& name,
    const std::any& value
)
{
    values[name] = value;
}

std::any Environment::get(
    const std::string& name
)
{
    auto it = values.find(name);

    if (it != values.end())
    {
        return it->second;
    }

    if (enclosing)
    {
        return enclosing->get(name);
    }

    throw std::runtime_error(
        "Undefined variable '" + name + "'."
    );
}

void Environment::assign(
    const std::string& name,
    const std::any& value
)
{
    auto it = values.find(name);

    if (it != values.end())
    {
        it->second = value;
        return;
    }

    if (enclosing)
    {
        enclosing->assign(name, value);
        return;
    }

    throw std::runtime_error(
        "Undefined variable '" + name + "'."
    );
}

std::any Environment::getAt(
    int distance,
    const std::string& name
)
{
    return ancestor(distance)->getValue(name);
}

void Environment::assignAt(
    int distance,
    const std::string& name,
    const std::any& value
)
{
    ancestor(distance)->values[name] = value;
}

std::shared_ptr<Environment> Environment::ancestor(
    int distance
)
{
    std::shared_ptr<Environment> environment =
        shared_from_this();

    for (int i = 0; i < distance; ++i)
    {
        environment = environment->enclosing;
    }

    return environment;
}

std::any Environment::getValue(
    const std::string& name
)
{
    auto it = values.find(name);

    if (it == values.end())
    {
        throw std::runtime_error(
            "Undefined variable '" + name + "'."
        );
    }

    return it->second;
}