// scientific.cpp —— 指数、对数、开方、幂、阶乘（定点十进制版）
#include "scientific.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sci {

using namespace fx;

//---------------------------------------------------------------------------//
//  常量（十进制展开）
//---------------------------------------------------------------------------//

static const char* PI_STR =
    "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068";
static const char* E_STR =
    "2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427";
static const char* LN2_STR =
    "0.693147180559945309417232121458176568075500134360255254120680009493393621969694715605863326996418688";

Dec pi() { return fromString(PI_STR); }
Dec e()  { return fromString(E_STR); }

//---------------------------------------------------------------------------//
//  内部工具
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
//  sqrt：牛顿迭代 x <- (x + a/x)/2
//---------------------------------------------------------------------------//

Dec sqrt(const Dec& x) {
    if (x.isZero()) return Dec();
    if (x.neg) throw std::runtime_error("sqrt 的参数不能为负数");
    // 初值取整数部分平方根的近似（用 double 估计即可，迭代会收敛）
    double approx = std::sqrt(std::stod(toString(x)));
    Dec cur = fromString(std::to_string(approx > 1 ? approx : 1.0));
    if (cur.isZero()) cur = fromInt(1);
    for (int i = 0; i < 200; ++i) {
        Dec next = div(add(cur, div(x, cur)), fromInt(2));
        if (cmp(next, cur) == 0) { cur = next; break; }
        cur = next;
    }
    return cur;
}

//---------------------------------------------------------------------------//
//  cbrt / root：牛顿迭代 x <- ((n-1)x + a/x^(n-1))/n
//---------------------------------------------------------------------------//

static Dec nthRoot(const Dec& a, long long n) {
    // 求 |a| 的 n 次方根（a > 0）
    double approx = std::pow(std::stod(toString(a)), 1.0 / (double)n);
    Dec cur = fromString(std::to_string(approx > 1 ? approx : 1.0));
    if (cur.isZero()) cur = fromInt(1);
    Dec nm1 = fromInt(n - 1);
    Dec nd  = fromInt(n);
    for (int i = 0; i < 300; ++i) {
        // x^(n-1)：用快速幂
        Dec p = fromInt(1);
        long long e = n - 1;
        Dec base = cur;
        while (e) {
            if (e & 1) p = mul(p, base);
            e >>= 1;
            if (e) base = mul(base, base);
        }
        Dec next = div(add(mul(nm1, cur), div(a, p)), nd);
        if (cmp(next, cur) == 0) { cur = next; break; }
        cur = next;
    }
    return cur;
}

Dec cbrt(const Dec& x) {
    if (x.isZero()) return Dec();
    Dec a = x.neg ? neg(x) : x;
    Dec r = nthRoot(a, 3);
    return x.neg ? neg(r) : r;
}

Dec root(const Dec& x, long long n) {
    if (n == 0) throw std::runtime_error("0 次方根无意义");
    if (n < 0) return div(fromInt(1), root(x, -n));
    if (n == 2) return sqrt(x);
    if (n == 3) return cbrt(x);
    if (x.neg) {
        if (n % 2 == 0) throw std::runtime_error("偶次方根的被开方数不能为负");
        return neg(nthRoot(neg(x), n));
    }
    if (x.isZero()) return Dec();
    return nthRoot(x, n);
}

//---------------------------------------------------------------------------//
//  ln：x = m * 2^e（m ∈ [1,2)），ln x = e*ln2 + 2*atanh((m-1)/(m+1))
//---------------------------------------------------------------------------//

Dec ln(const Dec& x) {
    if (x.isZero() || x.neg) throw std::runtime_error("ln 的参数必须为正数");

    // 归一化：反复乘/除 2，使 m ∈ [1,2)
    Dec m = x;
    long long e = 0;
    Dec two = fromInt(2);
    while (cmp(m, two) >= 0) { m = div(m, two); ++e; }
    while (cmp(m, fromInt(1)) < 0) { m = mul(m, two); --e; }

    // t = (m-1)/(m+1)
    Dec t = div(sub(m, fromInt(1)), add(m, fromInt(1)));
    Dec t2 = mul(t, t);

    // ln m = 2 * (t + t^3/3 + t^5/5 + ...)
    Dec sum = Dec();
    Dec term = t;
    Dec prev = Dec();
    for (int i = 1; i < 5000; ++i) {
        Dec add_ = div(term, fromInt(2 * i - 1));
        if (add_.isZero()) break;
        sum = fx::add(sum, add_);
        term = mul(term, t2);
        if (term.isZero()) break;
        if (cmp(sum, prev) == 0) break;
        prev = sum;
    }
    Dec lnm = mul(sum, fromInt(2));

    // ln x = e*ln2 + ln m
    Dec r = fx::add(mul(fromString(LN2_STR), fromInt(e)), lnm);
    return r;
}

Dec log2(const Dec& x)  { return div(ln(x), ln(fromInt(2))); }
Dec log10(const Dec& x) { return div(ln(x), div(ln(fromInt(10)), fromInt(1))); }
Dec logBase(const Dec& x, const Dec& base) {
    if (base.isZero() || base.neg) throw std::runtime_error("对数的底必须为正数");
    return div(ln(x), ln(base));
}

//---------------------------------------------------------------------------//
//  exp：先做范围缩减 x = k*ln2 + r，再对 r 用泰勒级数
//---------------------------------------------------------------------------//

Dec exp(const Dec& x) {
    if (x.isZero()) return fromInt(1);

    // 范围缩减：k = round(x / ln2)，r = x - k*ln2，|r| <= ln2/2
    Dec ln2 = fromString(LN2_STR);
    Dec kd = roundF(div(x, ln2));
    long long k = (long long)std::llround(std::stod(toString(kd)));
    Dec r = sub(x, mul(ln2, fromInt(k)));

    // exp r = 1 + r + r^2/2! + r^3/3! + ...
    Dec sum = fromInt(1);
    Dec term = fromInt(1);
    Dec prev = Dec();
    for (int n = 1; n < 5000; ++n) {
        term = div(mul(term, r), fromInt(n));
        if (term.isZero()) break;
        sum = fx::add(sum, term);
        if (cmp(sum, prev) == 0) break;
        prev = sum;
    }

    // exp x = exp(r) * 2^k
    Dec result = sum;
    Dec two = fromInt(2);
    if (k > 0) { for (long long i = 0; i < k; ++i) result = mul(result, two); }
    else       { for (long long i = 0; i < -k; ++i) result = div(result, two); }
    return result;
}

//---------------------------------------------------------------------------//
//  pow
//---------------------------------------------------------------------------//

Dec pow(const Dec& base, const Dec& exponent) {
    if (exponent.isZero()) return fromInt(1);
    if (base.isZero()) {
        if (exponent.neg) throw std::runtime_error("0 的负数次幂无意义");
        return Dec();
    }

    // 整数指数：用快速幂精确计算（结果精确，无超越函数误差）
    if (isInteger(exponent)) {
        long long n = std::llround(std::stod(toString(trunc(exponent))));
        if (n < 0) return div(fromInt(1), pow(base, fromInt(-n)));
        Dec result = fromInt(1);
        Dec b = base;
        long long e = n;
        while (e) {
            if (e & 1) {
                result = mul(result, b);
                requireResult(result, "幂运算结果");
            }
            e >>= 1;
            if (e) b = mul(b, b);
        }
        return result;
    }

    // 非整数指数：底数必须为正
    if (base.neg)
        throw std::runtime_error("负底数的非整数次幂在实数范围内无意义");
    return exp(mul(exponent, ln(base)));
}

//---------------------------------------------------------------------------//
//  阶乘
//---------------------------------------------------------------------------//

Dec factorial(long long n) {
    if (n < 0) throw std::runtime_error("阶乘的参数必须是非负整数");
    if (n > 100) throw std::runtime_error("阶乘的参数过大（n <= 100）");
    Dec acc = fromInt(1);
    for (long long i = 2; i <= n; ++i) acc = mul(acc, fromInt(i));
    return acc;
}

}  // namespace sci
