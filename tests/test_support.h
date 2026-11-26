#pragma once

// 回归测试共用的最小断言工具。
//
// 这些用例刻意不引入 Catch2/GTest：非 GUI 测试必须保持零 Qt、零第三方依赖，
// 否则拿到一台只装了编译器的机器就跑不起来。这里只提供三样东西：
//   - expect()：断言并打印一行 PASS/FAIL，失败时让进程最终返回 1；
//   - failures：当前测试进程累计的失败数；
//   - finish()：打印与旧用例一致的总结行并给出退出码。
//
// 每个测试都是独立可执行程序，正常只有一个翻译单元；failures 仍写成 inline
// 变量，方便以后把某个用例拆成多个 .cpp 而不必再改头文件。

#include <iostream>
#include <string_view>

namespace test_support {

/// @brief 当前测试进程累计的失败断言数。
inline int failures = 0;

/// @brief 断言条件成立，并打印一行人类可读的结果。
///
/// 无论成功失败都打印，这样 CTest 的失败输出里能直接看到执行到了哪一步；
/// 失败只累加计数、不抛异常，让同一个用例里的后续断言继续跑完，
/// 一次运行就能暴露多个问题。
/// @param condition 断言条件
/// @param what 这条断言在验证什么
inline void expect(bool condition, std::string_view what) {
    std::cout << (condition ? "PASS" : "FAIL") << ' ' << what << '\n';
    if (!condition) {
        ++failures;
    }
}

/// @brief 打印测试总结并返回适合 main 的退出码。
/// @return 全部通过返回 0，否则返回 1
inline int finish() {
    if (failures == 0) {
        std::cout << "ALL PASS\n";
        return 0;
    }
    std::cout << failures << " FAILURE(S)\n";
    return 1;
}

}  // namespace test_support
