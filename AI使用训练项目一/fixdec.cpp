// fixdec.cpp —— 定点十进制数实现
#include "fixdec.hpp"

#include <algorithm>
#include <stdexcept>

namespace fx {

using namespace bd;

//---------------------------------------------------------------------------//
//  全局小数位数
//---------------------------------------------------------------------------//

static int g_scale = 60;

int  scale() { return g_scale; }
void setScale(int digits) {
    if (digits < 1 || digits > 1000) throw std::runtime_error("小数位数必须在 1..1000");
    g_scale = digits;
}

//---------------------------------------------------------------------------//
//  内部工具
//---------------------------------------------------------------------------//

// 把 (mag, exp10) 形式的十进制值规整到 scale 位小数。
//   value = mag * 10^exp10 ，返回 |value| * 10^scale（四舍五入）
static Big toScaled(const Big& mag, int exp10, int scale, bool& neg, bool negIn) {
    neg = negIn;
    if (mag.empty()) { neg = false; return Big(); }
    int k = scale + exp10;
    if (k >= 0) return mulPow10(mag, k);              // 整数倍：精确
    // 需要丢掉 -k 位小数：取最高 keep 位（含四舍五入）即得 |value|*10^scale
    int keep = digitCount(mag) + k;
    if (keep <= 0) { neg = false; return Big(); }
    Big v = topDigits(mag, keep);
    // 四舍五入：被丢弃部分是否 >= 半个单位
    int drop = digitCount(mag) - keep;
    Big rest = bd::sub(mag, bd::mulPow10(v, drop));
    Big half = bd::mulPow10(Big{ 5 }, drop - 1);
    if (bd::cmp(rest, half) >= 0) v = bd::add(v, Big{ 1 });
    if (v.empty()) neg = false;
    return v;
}

//---------------------------------------------------------------------------//
//  构造
//---------------------------------------------------------------------------//

Dec zero() { return Dec(); }

Dec fromInt(long long v) {
    Dec r;
    if (v == 0) return r;
    r.neg = v < 0;
    uint64_t u = r.neg ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    r.mag = mulPow10(fromU64(u), g_scale);           // u * 10^SCALE
    return r;
}

Dec fromString(const std::string& s) {
    if (s.empty()) throw std::runtime_error("空数字");
    std::string mant = s, es;
    long long e10 = 0;
    size_t epos = s.find_first_of("eE");
    if (epos != std::string::npos) {
        mant = s.substr(0, epos);
        es = s.substr(epos + 1);
        e10 = std::stoll(es);
    }
    bool neg = false;
    if (!mant.empty() && (mant[0] == '-' || mant[0] == '+')) {
        neg = (mant[0] == '-');
        mant.erase(0, 1);
    }
    size_t dot = mant.find('.');
    std::string digits = mant;
    long long frac = 0;
    if (dot != std::string::npos) {
        digits = mant.substr(0, dot) + mant.substr(dot + 1);
        frac = (long long)(mant.size() - dot - 1);
    }
    if (digits.empty()) digits = "0";
    size_t nz = digits.find_first_not_of('0');
    if (nz == std::string::npos) return Dec();       // 全零
    digits = digits.substr(nz);

    Big mag = fromDecimalString(digits);
    Dec r;
    r.mag = toScaled(mag, (int)(e10 - frac), g_scale, r.neg, neg);
    return r;
}

std::string toString(const Dec& x) {
    if (x.isZero()) return "0";
    std::string body = toDecimalString(x.mag);       // 恰好有 SCALE 位小数
    int point = (int)body.size() - g_scale;          // 小数点位置
    std::string out;
    if (point <= 0) {
        out = "0." + std::string(-point, '0') + body;
    } else if (point >= (int)body.size()) {
        out = body;
    } else {
        out = body.substr(0, point) + "." + body.substr(point);
    }
    // 去掉小数部分的尾零
    if (out.find('.') != std::string::npos) {
        size_t last = out.find_last_not_of('0');
        if (out[last] == '.') --last;
        out = out.substr(0, last + 1);
    }
    return (x.neg ? "-" : "") + out;
}

//---------------------------------------------------------------------------//
//  判定与比较
//---------------------------------------------------------------------------//

int sign(const Dec& a) {
    if (a.isZero()) return 0;
    return a.neg ? -1 : 1;
}

int cmp(const Dec& a, const Dec& b) {
    if (a.isZero() && b.isZero()) return 0;
    if (a.neg != b.neg) return a.neg ? -1 : 1;
    int c = bd::cmp(a.mag, b.mag);
    return a.neg ? -c : c;
}

bool isInteger(const Dec& x) {
    if (x.isZero()) return true;
    int total = digitCount(x.mag);
    if (total <= g_scale) return false;               // |x| < 1 且非零
    Big ip = topDigits(x.mag, total - g_scale);
    return bd::cmp(mulPow10(ip, g_scale), x.mag) == 0;
}

//---------------------------------------------------------------------------//
//  四则
//---------------------------------------------------------------------------//

Dec neg(const Dec& a) {
    Dec r = a;
    if (!r.isZero()) r.neg = !r.neg;
    return r;
}

Dec add(const Dec& a, const Dec& b) {
    if (a.isZero()) return b;
    if (b.isZero()) return a;
    Dec r;
    if (a.neg == b.neg) {
        r.mag = bd::add(a.mag, b.mag);
        r.neg = a.neg;
    } else {
        int c = bd::cmp(a.mag, b.mag);
        if (c == 0) return Dec();
        if (c > 0) { r.mag = bd::sub(a.mag, b.mag); r.neg = a.neg; }
        else       { r.mag = bd::sub(b.mag, a.mag); r.neg = b.neg; }
    }
    if (r.mag.empty()) r.neg = false;
    return r;
}

Dec sub(const Dec& a, const Dec& b) { return add(a, neg(b)); }

Dec mul(const Dec& a, const Dec& b) {
    if (a.isZero() || b.isZero()) return Dec();
    // (am/10^S)*(bm/10^S) = am*bm / 10^S   （注意：此处 S = g_scale）
    // 即 am*bm 已有 2S 位小数，需要再除以 10^S 得到 S 位小数
    Big prod = bd::mul(a.mag, b.mag);                // 单位 10^(2S)
    Big den = bd::pow10(g_scale);
    Big q = bd::divDec(prod, den);
    Big rem = bd::sub(prod, bd::mul(q, den));
    if (bd::cmp(bd::mul(rem, Big{ 2 }), den) >= 0) q = bd::add(q, Big{ 1 });
    Dec r;
    r.neg = a.neg != b.neg;
    r.mag = q;
    if (r.mag.empty()) r.neg = false;
    return r;
}

Dec div(const Dec& a, const Dec& b) {
    if (b.isZero()) throw std::runtime_error("除以零");
    if (a.isZero()) return Dec();
    // (am/10^S) / (bm/10^S) = am / bm ；再放大 10^S 以保留 S 位小数
    Big num = bd::mulPow10(a.mag, g_scale);
    Big q = bd::divDec(num, b.mag);
    // 四舍五入
    Big rem = bd::sub(num, bd::mul(q, b.mag));
    if (bd::cmp(bd::mul(rem, Big{ 2 }), b.mag) >= 0) q = bd::add(q, Big{ 1 });
    Dec r;
    r.neg = a.neg != b.neg;
    r.mag = q;
    if (r.mag.empty()) r.neg = false;
    return r;
}

//---------------------------------------------------------------------------//
//  取整
//---------------------------------------------------------------------------//

Dec trunc(const Dec& x) {
    if (x.isZero()) return Dec();
    int total = digitCount(x.mag);
    if (total <= g_scale) return Dec();               // |x| < 1
    Big ip = topDigits(x.mag, total - g_scale);
    Dec r;
    r.neg = x.neg;
    r.mag = bd::mulPow10(ip, g_scale);
    if (r.mag.empty()) r.neg = false;
    return r;
}

Dec floorF(const Dec& x) {
    Dec t = trunc(x);
    if (!x.neg || x.isZero() || isInteger(x)) return t;
    return sub(t, fromInt(1));
}

Dec ceilF(const Dec& x) {
    Dec t = trunc(x);
    if (x.neg || x.isZero() || isInteger(x)) return t;
    return add(t, fromInt(1));
}

Dec roundF(const Dec& x) {
    Dec t = trunc(x);
    Dec f = sub(x, t);                                // 有符号小数部分
    Dec af;
    af.mag = f.mag;
    af.neg = false;
    if (af.isZero()) return t;
    Dec half = div(fromInt(1), fromInt(2));
    if (cmp(af, half) >= 0) return x.neg ? sub(t, fromInt(1)) : add(t, fromInt(1));
    return t;
}

Dec fraction(const Dec& x) { return sub(x, trunc(x)); }

//---------------------------------------------------------------------------//
//  范围校验
//---------------------------------------------------------------------------//

static void requireAbs(const Dec& x, long long limit, const char* what) {
    Dec lim = fromInt(limit);
    Dec ax;
    ax.mag = x.mag;
    if (cmp(ax, lim) > 0)
        throw std::runtime_error(std::string(what) + " 超出允许范围（|值| <= " +
                                 std::to_string(limit) + "）");
}

void requireInput(const Dec& x, const char* what)  { requireAbs(x, INPUT_LIMIT, what); }
void requireResult(const Dec& x, const char* what) { requireAbs(x, RESULT_LIMIT, what); }

}  // namespace fx
