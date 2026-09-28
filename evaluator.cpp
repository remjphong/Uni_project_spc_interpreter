#include "evaluator.h"
#include <iostream>
#include <stdexcept>

int Evaluator::evaluate(std::shared_ptr<ASTNode> node)
{

    if (node == nullptr) {
        throw std::runtime_error("AST node is null");
    }

    std::shared_ptr<NumberNode> numberNode =
        std::dynamic_pointer_cast<NumberNode>(node);

    if (numberNode) {
        return numberNode->value;
    }

    std::shared_ptr<BinaryOpNode> binaryNode =
        std::dynamic_pointer_cast<BinaryOpNode>(node);

    if (binaryNode) {

        int leftValue = evaluate(binaryNode->left);

        int rightValue = evaluate(binaryNode->right);

        if (binaryNode->operatorSymbol == "+") {
            return leftValue + rightValue;
        }

        if (binaryNode->operatorSymbol == "-") {
            return leftValue - rightValue;
        }

        if (binaryNode->operatorSymbol == "*") {
            return leftValue * rightValue;
        }

        if (binaryNode->operatorSymbol == "/") {

            if (rightValue == 0) {
                throw std::runtime_error("Cannot divide by zero");
            }

            return leftValue / rightValue;
        }

        throw std::runtime_error("Unknown operator");
    }


    throw std::runtime_error("Unknown AST node");
}