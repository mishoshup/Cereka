#include "script_interpreter.hpp"

#include <cctype>
#include <string>

// Expression evaluation for $ arithmetic and if-comparisons.
// Grammar: expr    = term (('+'|'-') term)*
//          term    = factor (('*'|'/') factor)*
//          factor  = NUMBER | IDENT | '(' expr ')' | '-' factor
// IDENT is resolved via numVariables (fallback to variables parsed as float).

namespace cereka {

float ScriptInterpreter::LookupNumVar(const std::string &name) const
{
    auto it = numVariables.find(name);
    if (it != numVariables.end())
        return it->second;
    auto sit = variables.find(name);
    if (sit != variables.end()) {
        try {
            return std::stof(sit->second);
        }
        catch (...) {
        }
    }
    return 0.0f;
}

namespace {

struct ExprParser {
    const std::string &src;
    size_t i = 0;
    const ScriptInterpreter &interp;

    ExprParser(const std::string &s, const ScriptInterpreter &v) : src(s), interp(v) {}

    void skipWs()
    {
        while (i < src.size() && std::isspace((unsigned char)src[i]))
            ++i;
    }

    float parseFactor()
    {
        skipWs();
        if (i >= src.size())
            return 0.0f;
        char c = src[i];
        if (c == '(') {
            ++i;
            float v = parseExpr();
            skipWs();
            if (i < src.size() && src[i] == ')')
                ++i;
            return v;
        }
        if (c == '-') {
            ++i;
            return -parseFactor();
        }
        if (std::isdigit((unsigned char)c) || c == '.') {
            size_t start = i;
            while (i < src.size() && (std::isdigit((unsigned char)src[i]) || src[i] == '.'))
                ++i;
            try {
                return std::stof(src.substr(start, i - start));
            }
            catch (...) {
                return 0.0f;
            }
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < src.size() &&
                   (std::isalnum((unsigned char)src[i]) || src[i] == '_'))
                ++i;
            return interp.LookupNumVar(src.substr(start, i - start));
        }
        ++i;  // skip unknown
        return 0.0f;
    }

    float parseTerm()
    {
        float lhs = parseFactor();
        while (true) {
            skipWs();
            if (i >= src.size())
                break;
            char c = src[i];
            if (c != '*' && c != '/')
                break;
            ++i;
            float rhs = parseFactor();
            if (c == '*')
                lhs = lhs * rhs;
            else
                lhs = (rhs != 0.0f) ? (lhs / rhs) : 0.0f;
        }
        return lhs;
    }

    float parseExpr()
    {
        float lhs = parseTerm();
        while (true) {
            skipWs();
            if (i >= src.size())
                break;
            char c = src[i];
            if (c != '+' && c != '-')
                break;
            ++i;
            float rhs = parseTerm();
            lhs = (c == '+') ? (lhs + rhs) : (lhs - rhs);
        }
        return lhs;
    }
};

}  // namespace

float ScriptInterpreter::EvalExpr(const std::string &expr) const
{
    ExprParser p(expr, *this);
    return p.parseExpr();
}

}  // namespace cereka
