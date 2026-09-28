class Resolver 
    : public ExprVisitor,
      public StmtVisitor 
{
private:
    using Scope = std::unordered_map<std::string, bool>;

    Interpreter& interpreter;
    std::vector<Scope> scopes;

    FunctionType currentFunction = Functiontype::NODE;

    ClassType currentClass = ClassType::NONE;
};

void beginScope() {
    scopes.emplace_back();
}
void endScope(){
    if (!scopes.empty()) {
        scopes.pop_back();
    }
}
void printScopes () {
    std::cout << "Scopes\n";
    for (int i = 0; i < static_cast<int>(scopes.size()); ++i) {
        std::cout<< "Scope " << i << ": ";
        for (const auto& [name, defined]: scopes[i]) {
            std::cout << name << "=" << (defined ? "true" : "false") 
            << " ";

        }

        std::cout << "\n";
    }
}