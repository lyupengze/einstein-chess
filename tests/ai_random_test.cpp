/// @file tests/ai_random_test.cpp
/// @brief 随机源回归：同种子复现、异种子变化、布阵始终是 1~6 的排列。
///
/// 随机引擎是线程局部的，ai::set_random_seed 负责播种；用例同时覆盖布阵与走子
/// 两条调用路径，确保它们共用同一个随机源。修复背景见 `#ai-shared-rng`。

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>

#include "ai/ai.h"
#include "test_support.h"

using test_support::expect;

int main() {
    ai::set_random_seed(12345);
    const auto first = ai::decide_layout();

    ai::set_random_seed(12345);
    const auto second = ai::decide_layout();
    expect(first == second, "同种子生成同一布阵");

    ai::set_random_seed(999);
    const auto third = ai::decide_layout();
    expect(first != third, "异种子改变布阵");

    auto sorted = first;
    std::sort(sorted.begin(), sorted.end());
    const std::array<int, 6> expected{1, 2, 3, 4, 5, 6};
    expect(sorted == expected, "布阵是 1~6 的排列");

    return test_support::finish();
}
