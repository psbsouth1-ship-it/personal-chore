// main.cpp —— 命令行入口
//
//   calc "2^10"            单次计算
//   calc -p 60 "1/3"       指定小数位数（默认 60，范围 1..1000）
//   calc                   交互模式（逐行读取算式）
//   calc -h / --help       帮助
#include <iostream>
#include <string>
#include <vector>

#include "fixdec.hpp"
#include "parser.hpp"

static void printHelp(const char* prog) {
    std::cout <<
        "高精度计算器（输入 |数字| <= 256，结果 <= 1024）\n"
        "\n"
        "用法:\n"
        "  " << prog << " [选项] \"算式\"     单次计算\n"
        "  " << prog << " [选项]              进入交互模式\n"
        "\n"
        "选项:\n"
        "  -p, --precision <位数>   小数位数（默认 60，范围 1..1000）\n"
        "  -h, --help               显示本帮助\n"
        "\n"
        "运算符:\n"
        "  + - * /        四则运算\n"
        "  //             整除（向下取整）\n"
        "  %              取余\n"
        "  ^              幂（右结合，如 2^10、7^(1/3)）\n"
        "  !              阶乘（后缀，如 10!）\n"
        "\n"
        "函数:\n"
        "  sqrt cbrt abs exp ln log(x[,base]) log2 log10\n"
        "  pow(x,y) root(x,n) trunc floor ceil round frac\n"
        "\n"
        "常量: pi e\n"
        "\n"
        "示例:\n"
        "  " << prog << " \"7*7\"\n"
        "       7*7\n"
        "       = 49\n"
        "  " << prog << " \"7^(1/3)\"\n"
        "       7^(1/3)\n"
        "       = 1.912931182772389101199116839548760282862439050345875766210652\n"
        "  " << prog << " \"100!\"\n";
}

// 计算一行算式并按面向用户的格式输出：
//
//     7*7
//     = 49
//
// 出错时同样先回显原式，再输出一行错误说明，保证顺序稳定。
// showExpr = false 时只输出结果行（交互模式下算式已显示在提示行）。
static int runLine(const std::string& line, bool showExpr = true) {
    if (showExpr) std::cout << line << "\n";
    try {
        fx::Dec r = par::eval(line);
        std::cout << "= " << fx::toString(r) << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "错误: " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char** argv) {
    int precision = 60;
    std::vector<std::string> exprs;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            printHelp(argv[0]);
            return 0;
        }
        if (a == "-p" || a == "--precision") {
            if (i + 1 >= argc) {
                std::cerr << "错误: " << a << " 需要一个参数\n";
                return 2;
            }
            try {
                precision = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "错误: 精度必须是整数\n";
                return 2;
            }
            if (precision < 1 || precision > 1000) {
                std::cerr << "错误: 精度必须在 1..1000 之间\n";
                return 2;
            }
            continue;
        }
        exprs.push_back(a);
    }

    try {
        fx::setScale(precision);
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << "\n";
        return 2;
    }

    if (!exprs.empty()) {
        std::string joined;
        for (size_t k = 0; k < exprs.size(); ++k) {
            if (k) joined += ' ';
            joined += exprs[k];
        }
        return runLine(joined, /*showExpr=*/true);
    }

    // 交互模式
    std::cout << "高精度计算器（输入 help 查看用法，quit 退出）\n";
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        // 去掉首尾空白
        size_t b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r\n");
        line = line.substr(b, e - b + 1);
        if (line == "quit" || line == "exit") break;
        if (line == "help") { printHelp("calc"); continue; }
        // 交互模式下算式已显示在提示行，无需重复回显
        runLine(line, /*showExpr=*/false);
    }
    return 0;
}
