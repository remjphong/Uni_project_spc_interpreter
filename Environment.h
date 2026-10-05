#pragma once

#include <any>
#include <memory>
#include <string>
#include <unordered_map>

class Environment
    : public std::enable_shared_from_this<Environment>
{
private:
    std::unordered_map<std::string, std::any> values;

    std::shared_ptr<Environment> enclosing;

public:
    Environment();

    explicit Environment(
        std::shared_ptr<Environment> enclosing
    );

    void define(
        const std::string& name,
        const std::any& value
    );

    std::any get(
        const std::string& name
    );

    void assign(
        const std::string& name,
        const std::any& value
    );

    std::any getAt(
        int distance,
        const std::string& name
    );

    void assignAt(
        int distance,
        const std::string& name,
        const std::any& value
    );

private:
    std::shared_ptr<Environment> ancestor(
        int distance
    );

    std::any getValue(
        const std::string& name
    );
};