// fixdec.hpp —— 定点十进制数
//
// 统一用「固定小数位数」表示实数：
//
//     值 = signed_mag / 10^SCALE
//
// signed_mag 是 bigdec::Big（十进制大整数，可带符号）。SCALE 为固定小数
// 位数（默认 60）。因为题目限定：
//
//     输入 |x| <= 2^8 = 256 ，结果 |y| <= 2^10 = 1024
//
// 整数部分至多 4 位，配合 SCALE 位小数，共约 64 位有效数字，精度绰绰有余，
// 且完全不需要二进制转换、定点二进制与保护位逻辑。
#ifndef FIXDEC_HPP
#define FIXDEC_HPP

#include <string>

#include "bigdec.hpp"

namespace fx {

using bd::Big;

// 固定小数位数
int  scale();
void setScale(int digits);

// 输入 / 结果的范围上限（2^8 / 2^10），超出即报错
constexpr long long INPUT_LIMIT  = 256;    // |输入| <= 256
constexpr long long RESULT_LIMIT = 1024;   // |结果| <= 1024

struct Dec {
    bool neg = false;
    Big  mag;                 // |值| * 10^SCALE

    bool isZero() const { return mag.empty(); }
};

// ---- 构造 ----------------------------------------------------------------
Dec  zero();
Dec  fromInt(long long v);
Dec  fromString(const std::string& s);      // "123.45" / "1e3" / ".5" / "-7"
std::string toString(const Dec& x);         // 去掉多余尾零的十进制字符串

// ---- 判定与比较 ---------------------------------------------------------
int  cmp(const Dec& a, const Dec& b);       // -1 / 0 / 1
int  sign(const Dec& a);
bool isInteger(const Dec& x);

// ---- 四则（结果自动四舍五入到 SCALE 位小数） ---------------------------
Dec  add(const Dec& a, const Dec& b);
Dec  sub(const Dec& a, const Dec& b);
Dec  mul(const Dec& a, const Dec& b);
Dec  div(const Dec& a, const Dec& b);       // 除零抛异常
Dec  neg(const Dec& a);

// ---- 取整 ----------------------------------------------------------------
Dec  trunc(const Dec& x);
Dec  floorF(const Dec& x);
Dec  ceilF(const Dec& x);
Dec  roundF(const Dec& x);
Dec  fraction(const Dec& x);

// ---- 范围校验 ------------------------------------------------------------
// |x| <= INPUT_LIMIT 时通过，否则抛异常
void requireInput(const Dec& x, const char* what);
// |x| <= RESULT_LIMIT 时通过，否则抛异常
void requireResult(const Dec& x, const char* what);

}  // namespace fx

#endif  // FIXDEC_HPP
