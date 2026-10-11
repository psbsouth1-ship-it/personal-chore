// parser.cpp —— 算式词法分析与求值
#include "parser.hpp"
#include "scientific.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace par {

using fx::Dec;

//===========================================================================//
//  词法分析
//===========================================================================//

enum class Tok {
    Number,    // 数字字面量
    Ident,     // 标识符（函数名或常量）
    Plus, Minus, Star, Slash, SlashSlash, Percent, Caret, Bang,
    LParen, RParen, Comma,
    End
};

struct Token {
    Tok  kind;
    std::string text;   // 数字原文 / 标识符
    size_t pos;         // 在原始串中的位置（用于报错）
};

static std::vector<Token> lex(const std::string& s) {
    std::vector<Token> out;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        if (std::isspace(c)) { ++i; continue; }

        // 数字：数字开头，或以 . 跟数字开头
        if (std::isdigit(c) || (c == '.' && i + 1 < s.size() && std::isdigit((unsigned char)s[i + 1]))) {
            size_t start = i;
            while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
            if (i < s.size() && s[i] == '.') {
                ++i;
                while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
            }
            // 指数部分
            if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                size_t save = i;
                ++i;
                if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
                if (i < s.size() && std::isdigit((unsigned char)s[i])) {
                    while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
                } else {
                    i = save;               // 不是合法指数，回退
                }
            }
            out.push_back({ Tok::Number, s.substr(start, i - start), start });
            continue;
        }

        // 标识符
        if (std::isalpha(c) || c == '_') {
            size_t start = i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_')) ++i;
            out.push_back({ Tok::Ident, s.substr(start, i - start), start });
            continue;
        }

        // 运算符
        Token t;
        t.pos = i;
        switch (s[i]) {
            case '+': t.kind = Tok::Plus;  break;
            case '-': t.kind = Tok::Minus; break;
            case '*': t.kind = Tok::Star;  break;
            case '%': t.kind = Tok::Percent; break;
            case '^': t.kind = Tok::Caret; break;
            case '!': t.kind = Tok::Bang;  break;
            case '(': t.kind = Tok::LParen; break;
            case ')': t.kind = Tok::RParen; break;
            case ',': t.kind = Tok::Comma;  break;
            case '/':
                if (i + 1 < s.size() && s[i + 1] == '/') { t.kind = Tok::SlashSlash; ++i; }
                else t.kind = Tok::Slash;
                break;
            default:
                throw std::runtime_error(std::string("无法识别的字符: '") + s[i] + "' (位置 " +
                                         std::to_string(i) + ")");
        }
        t.text = s.substr(t.pos, i - t.pos + 1);
        out.push_back(t);
        ++i;
    }
    out.push_back({ Tok::End, "", s.size() });
    return out;
}

//===========================================================================//
//  递归下降求值
//
//    expr    := term (('+' | '-') term)*
//    term    := unary (('*' | '/' | '//' | '%') unary)*
//    unary   := ('+' | '-') unary | power
//    power   := postfix ('^' unary)?          // 右结合
//    postfix := primary ('!')*
//    primary := number | ident | ident '(' args ')' | '(' expr ')'
//===========================================================================//

class Parser {
public:
    explicit Parser(std::vector<Token> toks) : t_(std::move(toks)) {}

    Dec parse() {
        Dec v = expr();
        if (peek().kind != Tok::End)
            throw std::runtime_error("表达式末尾有多余内容 (位置 " +
                                     std::to_string(peek().pos) + ")");
        fx::requireResult(v, "计算结果");
        return v;
    }

private:
    std::vector<Token> t_;
    size_t i_ = 0;

    const Token& peek() const { return t_[i_]; }
    const Token& next() { return t_[i_++]; }
    bool accept(Tok k) { if (t_[i_].kind == k) { ++i_; return true; } return false; }
    void expect(Tok k, const char* what) {
        if (t_[i_].kind != k)
            throw std::runtime_error(std::string("语法错误：期望 ") + what + " (位置 " +
                                     std::to_string(t_[i_].pos) + ")");
        ++i_;
    }

    // ---- expr ----
    Dec expr() {
        Dec v = term();
        for (;;) {
            if (accept(Tok::Plus))       v = fx::add(v, term());
            else if (accept(Tok::Minus)) v = fx::sub(v, term());
            else break;
            fx::requireResult(v, "中间结果");
        }
        return v;
    }

    // ---- term ----
    Dec term() {
        Dec v = unary();
        for (;;) {
            if (accept(Tok::Star)) {
                v = fx::mul(v, unary());
            } else if (accept(Tok::Slash)) {
                v = fx::div(v, unary());
            } else if (accept(Tok::SlashSlash)) {
                v = fx::floorF(fx::div(v, unary()));       // 整除（向下取整）
            } else if (accept(Tok::Percent)) {
                Dec r = unary();
                v = fx::sub(v, fx::mul(fx::trunc(fx::div(v, r)), r));   // 取余
            } else break;
            fx::requireResult(v, "中间结果");
        }
        return v;
    }

    // ---- unary ----
    Dec unary() {
        if (accept(Tok::Plus))  return unary();
        if (accept(Tok::Minus)) return fx::neg(unary());
        return power();
    }

    // ---- power（右结合） ----
    Dec power() {
        Dec base = postfix();
        if (accept(Tok::Caret)) {
            Dec e = unary();                 // 右结合，且允许 2^-3
            return sci::pow(base, e);
        }
        return base;
    }

    // ---- postfix（后缀阶乘） ----
    Dec postfix() {
        Dec v = primary();
        while (accept(Tok::Bang)) {
            if (!fx::isInteger(v))
                throw std::runtime_error("阶乘的参数必须是整数");
            long long n = (long long)std::llround(std::stod(fx::toString(fx::trunc(v))));
            v = sci::factorial(n);
            fx::requireResult(v, "阶乘结果");
        }
        return v;
    }

    // ---- primary ----
    Dec primary() {
        const Token& t = peek();
        if (t.kind == Tok::Number) {
            next();
            Dec v = fx::fromString(t.text);
            fx::requireInput(v, "输入数字");
            return v;
        }
        if (t.kind == Tok::LParen) {
            next();
            Dec v = expr();
            expect(Tok::RParen, "')'");
            return v;
        }
        if (t.kind == Tok::Ident) {
            next();
            std::string name = t.text;
            if (peek().kind == Tok::LParen) {
                next();
                std::vector<Dec> args;
                if (peek().kind != Tok::RParen) {
                    args.push_back(expr());
                    while (accept(Tok::Comma)) args.push_back(expr());
                }
                expect(Tok::RParen, "')'");
                return callFunction(name, args, t.pos);
            }
            return constant(name, t.pos);
        }
        throw std::runtime_error("语法错误：意外的记号 (位置 " + std::to_string(t.pos) + ")");
    }

    Dec constant(const std::string& name, size_t pos) {
        if (name == "pi") return sci::pi();
        if (name == "e")  return sci::e();
        throw std::runtime_error("未知的名称: " + name + " (位置 " + std::to_string(pos) + ")");
    }

    Dec callFunction(const std::string& name, std::vector<Dec>& args, size_t pos) {
        auto need = [&](size_t n, const char* sig) {
            if (args.size() != n)
                throw std::runtime_error(std::string("函数 ") + name + " 需要 " + sig +
                                         " 个参数 (位置 " + std::to_string(pos) + ")");
        };
        auto checkArg = [&](const Dec& v, size_t idx) {
            fx::requireInput(v, "函数参数");
            (void)idx;
        };

        if (name == "sqrt" || name == "cbrt" || name == "abs" ||
            name == "exp" || name == "ln" || name == "log2" || name == "log10" ||
            name == "trunc" || name == "floor" || name == "ceil" ||
            name == "round" || name == "frac") {
            need(1, "1");
            checkArg(args[0], 0);
            const Dec& x = args[0];
            if (name == "sqrt")  return sci::sqrt(x);
            if (name == "cbrt")  return sci::cbrt(x);
            if (name == "abs")   return x.neg ? fx::neg(x) : x;
            if (name == "exp")   return sci::exp(x);
            if (name == "ln")    return sci::ln(x);
            if (name == "log2")  return sci::log2(x);
            if (name == "log10") return sci::log10(x);
            if (name == "trunc") return fx::trunc(x);
            if (name == "floor") return fx::floorF(x);
            if (name == "ceil")  return fx::ceilF(x);
            if (name == "round") return fx::roundF(x);
            return fx::fraction(x);                    // frac
        }

        // log(x[, base])：一个参数时以 10 为底
        if (name == "log") {
            if (args.size() == 1) { checkArg(args[0], 0); return sci::log10(args[0]); }
            if (args.size() == 2) { checkArg(args[0], 0); checkArg(args[1], 1);
                                    return sci::logBase(args[0], args[1]); }
            throw std::runtime_error("函数 log 需要 1 或 2 个参数 (位置 " + std::to_string(pos) + ")");
        }

        // pow(base, exponent)
        if (name == "pow") {
            need(2, "2");
            checkArg(args[0], 0); checkArg(args[1], 1);
            return sci::pow(args[0], args[1]);
        }

        // root(x, n)：n 为整数
        if (name == "root") {
            need(2, "2");
            checkArg(args[0], 0);
            if (!fx::isInteger(args[1]))
                throw std::runtime_error("root 的次数必须是整数 (位置 " + std::to_string(pos) + ")");
            long long n = (long long)std::llround(std::stod(fx::toString(fx::trunc(args[1]))));
            return sci::root(args[0], n);
        }

        throw std::runtime_error("未知的函数: " + name + " (位置 " + std::to_string(pos) + ")");
    }
};

//===========================================================================//
//  对外接口
//===========================================================================//

Dec eval(const std::string& expression) {
    if (expression.empty()) throw std::runtime_error("空表达式");
    Parser p(lex(expression));
    return p.parse();
}

}  // namespace par
