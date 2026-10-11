@echo off
REM build.bat —— 编译高精度计算器
REM
REM Windows 控制台默认代码页为 GBK(936)，因此这里用 -fexec-charset=GBK
REM 让程序内嵌的中文字符串与代码页一致，避免中文乱码。
REM 如果改成用 chcp 65001 运行，可把 GBK 换成 UTF-8。

setlocal

set SRC=main.cpp parser.cpp scientific.cpp fixdec.cpp bigdec.cpp
set OUT=calc.exe

where g++ >nul 2>nul
if errorlevel 1 (
    echo [错误] 找不到 g++，请确认 MinGW 已加入 PATH
    exit /b 1
)

echo 正在编译 %OUT% ...
g++ -std=c++17 -Wall -Wextra -O2 -fexec-charset=GBK -o %OUT% %SRC%
if errorlevel 1 (
    echo [失败] 编译出错
    exit /b 1
)

echo [成功] 已生成 %OUT%
echo 运行示例:  %OUT% "1/3"
echo 交互模式:  %OUT%
endlocal
