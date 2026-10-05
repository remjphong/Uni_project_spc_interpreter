#include "Lexer.h"
#include "Parser.h"
#include "Resolver.h"
#include "Interpreter.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool readFile(const std::string& path, std::string& source)
{
    std::ifstream file(path);

    if (!file)
    {
        std::cerr << "Khong the mo file: " << path << '\n';
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    source = buffer.str();
    return true;
}

bool hasLexicalError(const std::vector<Token>& tokens)
{
    bool hasError = false;

    for (const auto& token : tokens)
    {
        if (token.type == TOKEN_ERROR)
        {
            std::cerr
                << "[Lexer Error] line " << token.line
                << ", column " << token.column
                << ": " << token.lexeme << '\n';

            hasError = true;
        }
    }

    return hasError;
}

void printTokens(const std::vector<Token>& tokens)
{
    std::cout << "===== TOKENS =====\n";

    for (const auto& token : tokens)
    {
        token.print();
    }

    std::cout << "==================\n\n";
}

const std::string& demoSource()
{
    static const std::string source =
        "var a = 10;\n"
        "var b = 20;\n"
        "print a + b * 2;\n"
        "if (a < b) {\n"
        "    print a;\n"
        "}\n"
        "while (a < 13) {\n"
        "    a = a + 1;\n"
        "    print a;\n"
        "}\n";

    return source;
}

void printUsage(const char* programName)
{
    std::cout
        << "Cach dung:\n"
        << "  " << programName << " <file.lox>\n"
        << "  " << programName << " --tokens <file.lox>\n"
        << "  " << programName << " --demo\n";
}

} // namespace

int main(int argc, char* argv[])
{
    std::string source;
    bool dumpTokens = false;

    if (argc == 1)
    {
        std::cout << "Khong co file dau vao -> chay demo mac dinh.\n\n";
        source = demoSource();
    }
    else if (argc == 2 && std::string(argv[1]) == "--demo")
    {
        source = demoSource();
    }
    else if (argc == 2)
    {
        if (!readFile(argv[1], source))
        {
            return 1;
        }
    }
    else if (argc == 3 && std::string(argv[1]) == "--tokens")
    {
        dumpTokens = true;

        if (!readFile(argv[2], source))
        {
            return 1;
        }
    }
    else
    {
        printUsage(argv[0]);
        return 1;
    }

    // ============================================================
    // 1. Lexer
    // ============================================================
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scanTokens();

    if (dumpTokens)
    {
        printTokens(tokens);
    }

    if (hasLexicalError(tokens))
    {
        std::cerr << "Dung chuong trinh do loi lexical.\n";
        return 1;
    }

    // ============================================================
    // 2. Parser
    // ============================================================
    Parser parser(std::move(tokens));
    std::vector<std::shared_ptr<Stmt>> statements = parser.parse();

    // ============================================================
    // 3. Resolver
    // ============================================================
    Interpreter interpreter;
    Resolver resolver(interpreter);

    resolver.resolve(statements);

    // ============================================================
    // 4. Interpreter
    // ============================================================
    interpreter.interpret(statements);

    return 0;
}
