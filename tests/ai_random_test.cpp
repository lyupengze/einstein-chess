// AI 随机源回归：BUG-10（函数内 static mt19937 不可复现、且多线程共享）。
//
// 修复后随机引擎是线程局部的，并提供 ai::set_random_seed 用于固定种子。
// 这里检查三件事：同种子结果一致、异种子结果不同、布阵始终是 1~6 的排列。

#include <algorithm>
#include <array>
#include <cstdio>

#include "ai/ai.h"

namespace {

int failures = 0;

void expect(bool cond, const char* what) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) {
        ++failures;
    }
}

}  // namespace

int main() {
    ai::set_random_seed(12345);
    const auto first = ai::decide_layout();

    ai::set_random_seed(12345);
    const auto second = ai::decide_layout();
    expect(first == second, "same seed reproduces the same layout");

    ai::set_random_seed(999);
    const auto third = ai::decide_layout();
    expect(first != third, "different seed changes the layout");

    auto sorted = first;
    std::sort(sorted.begin(), sorted.end());
    const std::array<int, 6> expected{1, 2, 3, 4, 5, 6};
    expect(sorted == expected, "layout is a permutation of 1..6");

    std::printf(failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
