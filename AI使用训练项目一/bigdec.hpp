// bigdec.hpp —— 任意精度十进制“整数”底层
//
// 只处理非负十进制整数，以 9 位十进制为一组（base = 1e9）的小端
// vector<uint32_t> 保存。所有函数都假定输入为标准形式：最高组非零，
// 空 vector 表示 0。
//
// 该层为上层高精度浮点（BigFloat）提供：
//   * 十进制加、减、乘
//   * 十进制长除（Knuth 算法 D）
//   * 十进制整数开平方
//   * 十进制数字组 <-> 二进制定点（32 位字）互转
#ifndef BIGDEC_HPP
#define BIGDEC_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace bd {

using Big = std::vector<uint32_t>;   // 小端，9 位十进制一组，每组 < 1e9

constexpr uint32_t BASE = 1000000000u;   // 1e9
constexpr int      D9   = 9;             // 每组十进制位数

// ---- 构造与转换 -----------------------------------------------------------
Big         fromU64(uint64_t v);
Big         fromDecimalString(const std::string& digits);  // 仅数字
std::string toDecimalString(const Big& a);                 // 无前导零

// ---- 判定与位数 -----------------------------------------------------------
bool isZero(const Big& a);
void normalize(Big& a);                 // 去掉高位零组
int  leadDigits(uint32_t group);        // 单个数字组的十进制位数
int  digitCount(const Big& a);          // 有效十进制位数
int  topDecExp(const Big& a);           // 最高位的十进指数

// ---- 比较 ----------------------------------------------------------------
int  cmp(const Big& a, const Big& b);   // -1 / 0 / 1

// ---- 加减乘 --------------------------------------------------------------
Big  add(const Big& a, const Big& b);
Big  sub(const Big& a, const Big& b);   // 要求 a >= b
Big  mul(const Big& a, const Big& b);
Big  mulSmall(const Big& a, uint64_t m);
Big  mulPow10(const Big& a, int k);     // a * 10^k
Big  mulPow2(const Big& a, int k);      // a * 2^k
Big  pow10(int k);                      // 10^k

// ---- 除法 ----------------------------------------------------------------
Big  divDec(const Big& a, const Big& b);   // floor(a / b)，b 为 0 抛异常

// ---- 开平方 --------------------------------------------------------------
Big  isqrt(const Big& a);                  // floor(sqrt(a))

// ---- 截断 ----------------------------------------------------------------
Big  topDigits(const Big& a, int n);       // 最高 n 位十进制数字（截断）

// ---- 舍入 ----------------------------------------------------------------
// 就地舍入到 n 位有效数字（十进制四舍五入，远离零）；保持数量级不变，
// 即结果会被补零回原来的位数。n <= 0 时清空。
void roundToDigits(Big& a, int n);

// ---- 十进制 <-> 二进制定点 ----------------------------------------------
std::vector<uint32_t> toBin(const Big& a);
Big  fromBin(const std::vector<uint32_t>& w, bool roundUp = false);

// ---- 二进制辅助（小端 32 位字） -----------------------------------------
void stripHi(std::vector<uint32_t>& v);
int  bitLen(const std::vector<uint32_t>& v);
std::vector<uint32_t> binShiftL(const std::vector<uint32_t>& v, int k);
std::vector<uint32_t> binShiftR(const std::vector<uint32_t>& v, int k);
std::vector<uint32_t> binAdd(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b);
std::vector<uint32_t> binSub(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b);   // 要求 a >= b
int  binCmp(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);
std::vector<uint32_t> binMul(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b);
std::vector<uint32_t> binDiv(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b,
                             bool* remNonZero = nullptr);

}  // namespace bd

#endif  // BIGDEC_HPP
