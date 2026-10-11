// scientific.hpp —— 指数、对数、开方、幂、阶乘（定点十进制版）
//
// 在 fixdec 提供的定点十进制（默认 60 位小数）之上实现：
//   * sqrt / cbrt / root ：牛顿迭代
//   * ln / exp           ：十进制级数（区间小，收敛快）
//   * pow                ：exp(y*ln x)
//   * factorial          ：连乘
//
// 由题目限定（输入 |x| <= 256、结果 |y| <= 1024），整数部分很小，
// 因此不需要二进制换算与定点二进制，实现显著简化。
#ifndef SCIENTIFIC_HPP
#define SCIENTIFIC_HPP

#include "fixdec.hpp"

namespace sci {

using fx::Dec;

// ---- 常量 ----------------------------------------------------------------
Dec pi();
Dec e();

// ---- 开方 ----------------------------------------------------------------
Dec sqrt(const Dec& x);                   // x >= 0
Dec cbrt(const Dec& x);                   // 允许负数
Dec root(const Dec& x, long long n);      // x 的 n 次方根

// ---- 指数与对数 ---------------------------------------------------------
Dec exp(const Dec& x);                    // |x| <= 256
Dec ln(const Dec& x);                     // x > 0
Dec log2(const Dec& x);
Dec log10(const Dec& x);
Dec logBase(const Dec& x, const Dec& base);
Dec pow(const Dec& base, const Dec& exponent);

// ---- 阶乘 ----------------------------------------------------------------
Dec factorial(long long n);               // n 为 0..100

}  // namespace sci

#endif  // SCIENTIFIC_HPP
