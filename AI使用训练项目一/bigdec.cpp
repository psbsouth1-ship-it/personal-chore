// bigdec.cpp —— 任意精度十进制整数底层实现
#include "bigdec.hpp"

#include <algorithm>
#include <stdexcept>

namespace bd {

//---------------------------------------------------------------------------//
//  构造与转换
//---------------------------------------------------------------------------//

Big fromU64(uint64_t v) {
    Big r;
    while (v) { r.push_back((uint32_t)(v % BASE)); v /= BASE; }
    return r;
}

Big fromDecimalString(const std::string& digits) {
    Big r;
    for (int i = (int)digits.size(); i > 0; i -= D9) {
        int start = std::max(0, i - D9);
        r.push_back((uint32_t)std::stoul(digits.substr(start, i - start)));
    }
    normalize(r);
    return r;
}

std::string toDecimalString(const Big& a) {
    if (a.empty()) return "0";
    std::string s = std::to_string(a.back());
    for (int i = (int)a.size() - 2; i >= 0; --i) {
        std::string g = std::to_string(a[i]);
        s += std::string(D9 - g.size(), '0') + g;
    }
    return s;
}

//---------------------------------------------------------------------------//
//  判定与位数
//---------------------------------------------------------------------------//

bool isZero(const Big& a) { return a.empty(); }

void normalize(Big& a) { while (!a.empty() && a.back() == 0) a.pop_back(); }

int leadDigits(uint32_t g) {
    int n = 1;
    while (g >= 10) { g /= 10; ++n; }
    return n;
}

int digitCount(const Big& a) {
    if (a.empty()) return 0;
    return (int)a.size() * D9 - D9 + leadDigits(a.back());
}

int topDecExp(const Big& a) {
    if (a.empty()) return 0;
    return (int)(a.size() - 1) * D9 + leadDigits(a.back()) - 1;
}

//---------------------------------------------------------------------------//
//  比较
//---------------------------------------------------------------------------//

int cmp(const Big& a, const Big& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0;)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

//---------------------------------------------------------------------------//
//  加减乘
//---------------------------------------------------------------------------//

Big add(const Big& a, const Big& b) {
    Big r(std::max(a.size(), b.size()) + 1, 0);
    uint32_t carry = 0;
    for (size_t i = 0; i < r.size(); ++i) {
        uint64_t s = carry;
        if (i < a.size()) s += a[i];
        if (i < b.size()) s += b[i];
        r[i] = (uint32_t)(s % BASE);
        carry = (uint32_t)(s / BASE);
    }
    normalize(r);
    return r;
}

Big sub(const Big& a, const Big& b) {
    Big r(a.size(), 0);
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t cur = (int64_t)a[i] - borrow - (i < b.size() ? (int64_t)b[i] : 0);
        if (cur < 0) { cur += BASE; borrow = 1; } else borrow = 0;
        r[i] = (uint32_t)cur;
    }
    normalize(r);
    return r;
}

Big mul(const Big& a, const Big& b) {
    if (a.empty() || b.empty()) return Big();
    Big r(a.size() + b.size(), 0);
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size(); ++j) {
            uint64_t cur = (uint64_t)r[i + j] + (uint64_t)a[i] * b[j] + carry;
            r[i + j] = (uint32_t)(cur % BASE);
            carry    = cur / BASE;
        }
        size_t k = i + b.size();
        while (carry) {
            uint64_t cur = (uint64_t)r[k] + carry;
            r[k] = (uint32_t)(cur % BASE);
            carry = cur / BASE;
            ++k;
        }
    }
    normalize(r);
    return r;
}

Big mulSmall(const Big& a, uint64_t m) {
    if (a.empty() || m == 0) return Big();
    Big r(a.size() + 1, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t cur = (uint64_t)a[i] * m + carry;
        r[i] = (uint32_t)(cur % BASE);
        carry = cur / BASE;
    }
    size_t k = a.size();
    while (carry) {
        uint64_t cur = (uint64_t)r[k] + carry;
        r[k] = (uint32_t)(cur % BASE);
        carry = cur / BASE;
        ++k;
    }
    normalize(r);
    return r;
}

Big mulPow10(const Big& a, int k) {
    if (a.empty() || k <= 0) return a;
    int groups = k / D9, rem = k % D9;
    Big r(a.size() + groups + 1, 0);
    for (size_t i = 0; i < a.size(); ++i) r[i + groups] = a[i];
    normalize(r);
    if (rem == 0) return r;
    uint64_t p = 1;
    for (int i = 0; i < rem; ++i) p *= 10;
    return mulSmall(r, p);
}

Big mulPow2(const Big& a, int k) {
    Big r = a;
    while (k > 0) { int t = std::min(k, 29); r = mulSmall(r, 1ull << t); k -= t; }
    return r;
}

Big pow10(int k) {
    if (k < 0) return Big();
    return mulPow10(Big{ 1 }, k);
}

//---------------------------------------------------------------------------//
//  十进制长除法（Knuth 算法 D）
//---------------------------------------------------------------------------//

Big divDec(const Big& a, const Big& b) {
    if (b.empty()) throw std::runtime_error("除以零");
    if (cmp(a, b) < 0) return Big();

    // 单组除数：一次遍历即可
    if (b.size() == 1) {
        Big q(a.size(), 0);
        uint64_t rem = 0;
        for (int i = (int)a.size() - 1; i >= 0; --i) {
            uint64_t cur = rem * BASE + a[i];
            q[i] = (uint32_t)(cur / b[0]);
            rem = cur % b[0];
        }
        normalize(q);
        return q;
    }

    // 归一化：让除数的最高组 >= BASE/2
    uint64_t norm = BASE / ((uint64_t)b.back() + 1);
    Big v = (norm == 1) ? b : mulSmall(b, norm);
    Big w = (norm == 1) ? a : mulSmall(a, norm);
    if (w.size() < v.size()) w.push_back(0);

    const int n = (int)v.size();
    int m = (int)w.size() - n;
    Big q(m, 0);
    for (int j = m - 1; j >= 0; --j) {
        uint64_t num = (uint64_t)w[j + n] * BASE + w[j + n - 1];
        uint64_t qhat = num / v[n - 1];
        uint64_t rhat = num % v[n - 1];
        while (qhat >= BASE || qhat * v[n - 2] > rhat * BASE + w[j + n - 2]) {
            --qhat;
            rhat += v[n - 1];
            if (rhat >= BASE) break;
        }
        // w[j .. j+n] -= qhat * v
        int64_t borrow = 0;
        uint64_t carry = 0;
        for (int i = 0; i < n; ++i) {
            uint64_t p = qhat * v[i] + carry;
            carry = p / BASE;
            int64_t t = (int64_t)w[j + i] - (int64_t)(p % BASE) - borrow;
            if (t < 0) { t += BASE; borrow = 1; } else borrow = 0;
            w[j + i] = (uint32_t)t;
        }
        int64_t t = (int64_t)w[j + n] - (int64_t)carry - borrow;
        if (t < 0) { t += BASE; borrow = 1; } else borrow = 0;
        w[j + n] = (uint32_t)t;
        if (borrow) {                       // 试商偏大，回退一步
            --qhat;
            uint64_t c = 0;
            for (int i = 0; i < n; ++i) {
                uint64_t s = (uint64_t)w[j + i] + v[i] + c;
                w[j + i] = (uint32_t)(s % BASE);
                c = s / BASE;
            }
            w[j + n] = (uint32_t)((uint64_t)w[j + n] + c);
        }
        q[j] = (uint32_t)qhat;
    }
    normalize(q);
    return q;
}

//---------------------------------------------------------------------------//
//  整数开平方（牛顿迭代 + 校正）
//---------------------------------------------------------------------------//

Big isqrt(const Big& a) {
    if (a.empty()) return Big();
    int d = digitCount(a);
    Big x = mulPow10(Big{ 3 }, (d + 1) / 2);     // 初值 3 * 10^ceil(d/2)，偏大
    while (true) {
        Big q = divDec(a, x);
        Big s = add(x, q);
        Big half;
        uint64_t rem = 0;
        for (int i = (int)s.size() - 1; i >= 0; --i) {
            uint64_t cur = rem * BASE + s[i];
            half.push_back((uint32_t)(cur / 2));
            rem = cur % 2;
        }
        std::reverse(half.begin(), half.end());
        normalize(half);
        if (cmp(half, x) >= 0) break;            // 不再下降
        x = half;
    }
    // 校正到 floor(sqrt(a))
    while (!x.empty() && cmp(mul(x, x), a) > 0) x = sub(x, Big{ 1 });
    while (true) {
        Big x1 = add(x, Big{ 1 });
        if (cmp(mul(x1, x1), a) <= 0) x = x1;
        else break;
    }
    return x;
}

//---------------------------------------------------------------------------//
//  截断与舍入
//---------------------------------------------------------------------------//

Big topDigits(const Big& a, int n) {
    if (a.empty() || n <= 0) return Big();
    int total = digitCount(a);
    if (n >= total) return a;
    int drop = total - n;
    Big r = a;
    int fullGroups = drop / D9, remDigits = drop % D9;
    if (fullGroups > 0) {
        if (fullGroups >= (int)r.size()) return Big();
        r.erase(r.begin(), r.begin() + fullGroups);
    }
    if (remDigits > 0) {
        uint32_t m = 1;
        for (int i = 0; i < remDigits; ++i) m *= 10;
        Big q;
        uint64_t rem = 0;
        for (int i = (int)r.size() - 1; i >= 0; --i) {
            uint64_t cur = rem * BASE + r[i];
            q.push_back((uint32_t)(cur / m));
            rem = cur % m;
        }
        std::reverse(q.begin(), q.end());
        normalize(q);
        r = q;
    }
    return r;
}

void roundToDigits(Big& a, int n) {
    if (n <= 0) { a.clear(); return; }
    if (a.empty()) return;
    int total = digitCount(a);
    if (total <= n) return;
    int D = total - n;
    Big t = topDigits(a, n);
    Big rest = sub(a, mulPow10(t, D));            // 被丢弃的部分
    Big half = mulPow10(Big{ 5 }, D - 1);         // 半个单位
    if (cmp(rest, half) >= 0) {
        Big inc = add(t, Big{ 1 });
        if (digitCount(inc) > n) { t = topDigits(inc, n); ++D; }
        else t = inc;
    }
    a = mulPow10(t, D);                           // 补零回原数量级
}

//---------------------------------------------------------------------------//
//  十进制数字组 <-> 二进制定点（小端 32 位字）
//---------------------------------------------------------------------------//

std::vector<uint32_t> toBin(const Big& a) {
    std::vector<uint32_t> out;
    if (a.empty()) return out;
    // out = 0；对每个数字组： out = out * 1e9 + group（base 2^32）
    for (int i = (int)a.size() - 1; i >= 0; --i) {
        uint64_t carry = a[i];
        for (size_t w = 0; w < out.size(); ++w) {
            uint64_t cur = (uint64_t)out[w] * 1000000000ull + carry;
            out[w] = (uint32_t)cur;
            carry  = cur >> 32;
        }
        while (carry) {
            out.push_back((uint32_t)carry);
            carry >>= 32;
        }
    }
    stripHi(out);
    return out;
}

Big fromBin(const std::vector<uint32_t>& w, bool roundUp) {
    Big res;
    // res = 0；对每个二进制字（从高到低）：res = res * 2^32 + word（base 1e9）
    for (int i = (int)w.size() - 1; i >= 0; --i) {
        uint64_t carry = w[i];
        for (size_t j = 0; j < res.size(); ++j) {
            uint64_t cur = (uint64_t)res[j] * 4294967296ull + carry;
            res[j] = (uint32_t)(cur % BASE);
            carry  = cur / BASE;
        }
        while (carry) { res.push_back((uint32_t)(carry % BASE)); carry /= BASE; }
    }
    normalize(res);
    if (roundUp && !res.empty()) res = add(res, Big{ 1 });
    return res;
}

//---------------------------------------------------------------------------//
//  二进制辅助
//---------------------------------------------------------------------------//

void stripHi(std::vector<uint32_t>& v) { while (!v.empty() && v.back() == 0) v.pop_back(); }

int bitLen(const std::vector<uint32_t>& v) {
    if (v.empty()) return 0;
    uint32_t top = v.back();
    int b = 0;
    while (top) { ++b; top >>= 1; }
    return (int)(v.size() - 1) * 32 + b;
}

std::vector<uint32_t> binShiftL(const std::vector<uint32_t>& a, int k) {
    if (a.empty() || k <= 0) return a;
    int w = k / 32, s = k % 32;
    std::vector<uint32_t> r(a.size() + w + 1, 0);
    for (size_t i = 0; i < a.size(); ++i) r[i + w] = a[i];
    if (s) {
        uint32_t carry = 0;
        for (size_t i = 0; i < r.size(); ++i) {
            uint64_t cur = ((uint64_t)r[i] << s) | carry;
            r[i] = (uint32_t)cur;
            carry = (uint32_t)(cur >> 32);
        }
        if (carry) r.push_back(carry);
    }
    stripHi(r);
    return r;
}

std::vector<uint32_t> binShiftR(const std::vector<uint32_t>& a, int k) {
    if (k <= 0) return a;
    int w = k / 32, s = k % 32;
    std::vector<uint32_t> r(a.size(), 0);
    for (size_t i = 0; i + w < a.size(); ++i) {
        uint64_t v = (uint64_t)a[i + w] >> s;
        if (s && i + w + 1 < a.size()) v |= (uint64_t)a[i + w + 1] << (32 - s);
        r[i] = (uint32_t)v;
    }
    stripHi(r);
    return r;
}

std::vector<uint32_t> binAdd(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r(std::max(a.size(), b.size()) + 1, 0);
    uint32_t carry = 0;
    for (size_t i = 0; i < r.size(); ++i) {
        uint64_t s = carry;
        if (i < a.size()) s += a[i];
        if (i < b.size()) s += b[i];
        r[i] = (uint32_t)s;
        carry = (uint32_t)(s >> 32);
    }
    stripHi(r);
    return r;
}

std::vector<uint32_t> binSub(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r(a.size(), 0);
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t cur = (int64_t)a[i] - borrow - (i < b.size() ? (int64_t)b[i] : 0);
        if (cur < 0) { cur += 4294967296ll; borrow = 1; } else borrow = 0;
        r[i] = (uint32_t)cur;
    }
    stripHi(r);
    return r;
}

int binCmp(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0;)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

std::vector<uint32_t> binMul(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b) {
    if (a.empty() || b.empty()) return {};
    std::vector<uint32_t> r(a.size() + b.size(), 0);
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size(); ++j) {
            uint64_t cur = (uint64_t)r[i + j] + (uint64_t)a[i] * b[j] + carry;
            r[i + j] = (uint32_t)cur;
            carry    = cur >> 32;
        }
        size_t k = i + b.size();
        while (carry) {
            uint64_t cur = (uint64_t)r[k] + carry;
            r[k] = (uint32_t)cur;
            carry = cur >> 32;
            ++k;
        }
    }
    stripHi(r);
    return r;
}

std::vector<uint32_t> binDiv(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b,
                             bool* remNonZero) {
    if (b.empty()) throw std::runtime_error("除以零");
    // 逐位长除：从最高位到最低位，商在比特位 i 处置位
    int abits = bitLen(a);
    std::vector<uint32_t> q((abits + 31) / 32, 0), rem;
    for (int i = abits - 1; i >= 0; --i) {
        // rem = rem*2 + bit_i(a)
        rem = binShiftL(rem, 1);
        if ((a[i / 32] >> (i % 32)) & 1u) {
            if (rem.empty()) rem.push_back(1);
            else rem[0] |= 1u;
        }
        if (binCmp(rem, b) >= 0) {
            rem = binSub(rem, b);
            q[i / 32] |= (1u << (i % 32));
        }
    }
    stripHi(q);
    if (remNonZero) *remNonZero = !rem.empty();
    return q;
}

}  // namespace bd
