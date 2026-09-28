#ifndef EVALUATOR_H
#define EVALUATOR_H

// 
#include <memory>

class Evaluator {
public:
    int evaluate(std::shared_ptr<ASTNode> node);
};

#endif