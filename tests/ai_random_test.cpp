/// @file tests/ai_random_test.cpp
/// @brief 随机源回归：同种子复现、异种子变化、布阵始终是 1~6 的排列。
///
/// AI 只保存一个种子，抽签引擎按“种子 + 局面”现构造；用例同时覆盖布阵与走子
/// 两条调用路径，并锁定“同一局面重复调用给出同一决策”（读档续玩依赖这一条）。
/// 修复背景见 `#ai-shared-rng`。

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>

#include "ai/ai.h"
#include "core/game_state.h"
#include "core/types.h"
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

    core::GameState state;
    state.board.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});
    state.board.setup_custom(core::Side::Blue, {1, 2, 3, 4, 5, 6});
    state.phase = core::Phase::Player1Thinking;
    state.current_side = core::Side::Red;
    state.current_dice = 3;

    ai::set_random_seed(777);
    const core::Move random_first = ai::decide_move(state, ai::Level::Random);
    const core::Move random_second = ai::decide_move(state, ai::Level::Random);
    expect(random_first == random_second, "随机档重复决策不漂移");

    const core::Move search_first = ai::decide_move(state, ai::Level::Search);
    const core::Move search_second = ai::decide_move(state, ai::Level::Search);
    expect(search_first == search_second, "搜索档的平局抽签不随调用次数漂移");

    ai::set_random_seed(777);
    expect(ai::decide_move(state, ai::Level::Random) == random_first, "重设种子后随机档结果一致");
    expect(ai::decide_move(state, ai::Level::Search) == search_first, "重设种子后搜索档结果一致");

    return test_support::finish();
}
