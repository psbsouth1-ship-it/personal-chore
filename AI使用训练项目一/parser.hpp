// parser.hpp —— 算式词法分析与求值
//
// 支持：
//   数字   123 / 1.5 / .5 / 1e3 / 1.2e-4
//   运算符 + - * /  //  %  ^（右结合）  !（后缀阶乘）
//   括号   ( )
//   一元   -x  +x
//   函数   sqrt cbrt abs exp ln log log2 log10 pow root trunc floor ceil round frac
//   常量   pi e
//
// 输入限制：单个数字 |x| <= 256；计算结果 |y| <= 1024。
#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>

#include "fixdec.hpp"

namespace par {

// 计算一个算式字符串，返回定点十进制结果。
// 语法错误或数值越界时抛 std::runtime_error。
fx::Dec eval(const std::string& expression);

}  // namespace par

#endif  // PARSER_HPP
