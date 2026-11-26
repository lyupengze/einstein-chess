/// @file tests/ai_level_test.cpp
/// @brief AI 档位回归：默认档、显式档位分发与公共契约。
///
/// 这里不评价两档的棋力强弱（那需要成规模的整局统计），只固化接口契约：
/// 省略档位等于传 kDefaultLevel；显式档位必须真的走到对应实现；非法档位抛异常。

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "ai/ai.h"
#include "core/board.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"
#include "test_support.h"

using test_support::expect;

namespace {

// 红方 1 号在 (3,3)，可以斜进 (4,4) 一步获胜；随机档在该局面也有多个候选。
core::GameState make_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 1;
    state.board.at({3, 3}) = {core::Side::Red, 1};
    state.board.at({0, 4}) = {core::Side::Blue, 1};
    state.board.at({1, 4}) = {core::Side::Blue, 2};
    return state;
}

bool is_legal(const core::GameState& state, const core::Move& move) {
    return core::rules::check_move(state, move.from, move.to).empty();
}

bool rejects_bad_phase(ai::Level level) {
    core::GameState idle;
    try {
        [[maybe_unused]] const core::Move ignored = ai::decide_move(idle, level);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    const core::GameState state = make_state();

    // 1. 默认档必须是搜索档。
    expect(ai::kDefaultLevel == ai::Level::Search, "默认档指向搜索档");

    // 2. 省略档位与显式传默认档必须走出同一步（固定种子下完全可复现）。
    ai::set_random_seed(2026);
    const core::Move by_default = ai::decide_move(state);
    ai::set_random_seed(2026);
    const core::Move by_latest = ai::decide_move(state, ai::kDefaultLevel);
    expect(by_default == by_latest, "省略档位与显式传 kDefaultLevel 结果一致");

    // 3. 每个档位都必须基于 core 的合法着法集合做选择。
    for (ai::Level level : {ai::Level::Random, ai::Level::Search}) {
        ai::set_random_seed(7);
        const core::Move move = ai::decide_move(state, level);
        expect(is_legal(state, move),
               std::string("所选档位返回合法着法：") + ai::level_name(level));
    }

    // 4. 显式选搜索档时，一步获胜的着法不会被同分随机抹掉。
    {
        ai::set_random_seed(7);
        const core::Move move = ai::decide_move(state, ai::Level::Search);
        const core::Move expected{{3, 3}, {4, 4}};
        expect(move == expected, "搜索档不会放弃一步获胜的着法");
    }

    // 5. 随机档：同一种子重放必须得到同一步，证明档位分发接入了共享随机源。
    {
        ai::set_random_seed(123);
        const core::Move first = ai::decide_move(state, ai::Level::Random);
        ai::set_random_seed(123);
        const core::Move second = ai::decide_move(state, ai::Level::Random);
        expect(first == second, "随机档在固定种子下可复现");
    }

    // 6. 越界的档位值必须显式拒绝，而不是静默降级到某个档位。
    {
        bool unknown_threw = false;
        try {
            [[maybe_unused]] const core::Move ignored =
                ai::decide_move(state, static_cast<ai::Level>(99));
        } catch (const std::invalid_argument&) {
            unknown_threw = true;
        }
        expect(unknown_threw, "未知档位被拒绝");
    }

    // 7. 两个档位的阶段校验保持一致。
    expect(rejects_bad_phase(ai::Level::Random), "随机档拒绝非思考阶段");
    expect(rejects_bad_phase(ai::Level::Search), "搜索档拒绝非思考阶段");

    return test_support::finish();
}
