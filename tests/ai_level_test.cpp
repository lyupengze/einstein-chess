// AI 版本选择回归：验证公共接口默认使用最新版、显式版本可切换、
// 第一版随机实现在固定种子下可复现，并且两个版本都只产合法着法。
//
// 这里不评价两代 AI 的棋力强弱（那需要成规模的整局统计），只固化接口契约：
// 不写版本 == 写当前最新版；显式版本必须真的走到对应实现。

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

namespace {

int failures = 0;

void expect(bool cond, std::string_view what) {
    std::cout << (cond ? "PASS" : "FAIL") << ' ' << what << '\n';
    if (!cond) {
        ++failures;
    }
}

// 红方 1 号在 (3,3)，可以斜进 (4,4) 一步获胜；随机版在该局面也有多个候选。
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
        (void)ai::decide_move(idle, level);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    const core::GameState state = make_state();

    // 1. 默认版本必须是当前最新版（第二版搜索）。
    expect(ai::kDefaultLevel == ai::Level::kSearch,
           "default level points at the latest search AI");

    // 2. 不写版本与显式写默认版本必须走出同一步（固定种子下完全可复现）。
    ai::set_random_seed(2026);
    const core::Move by_default = ai::decide_move(state);
    ai::set_random_seed(2026);
    const core::Move by_latest = ai::decide_move(state, ai::kDefaultLevel);
    expect(by_default == by_latest, "omitting the level equals passing kDefaultLevel");

    // 3. 每个档位都必须基于 core 的合法着法集合做选择。
    for (ai::Level level : {ai::Level::kRandom, ai::Level::kSearch}) {
        ai::set_random_seed(7);
        const core::Move move = ai::decide_move(state, level);
        expect(is_legal(state, move),
               std::string("selected level returns a legal move: ") + ai::level_name(level));
    }

    // 4. 显式选第二版时，一步获胜的着法不会被同分随机抹掉。
    {
        ai::set_random_seed(7);
        const core::Move move = ai::decide_move(state, ai::Level::kSearch);
        const core::Move expected{{3, 3}, {4, 4}};
        expect(move == expected, "search level still takes the immediate win");
    }

    // 5. 第一版随机：同一种子重放必须得到同一步，证明版本分发接入了共享随机源。
    {
        ai::set_random_seed(123);
        const core::Move first = ai::decide_move(state, ai::Level::kRandom);
        ai::set_random_seed(123);
        const core::Move second = ai::decide_move(state, ai::Level::kRandom);
        expect(first == second, "random level is reproducible with a fixed seed");
    }

    // 6. 越界的版本号必须显式拒绝，而不是静默降级到某个版本。
    {
        bool unknown_threw = false;
        try {
            (void)ai::decide_move(state, static_cast<ai::Level>(99));
        } catch (const std::invalid_argument&) {
            unknown_threw = true;
        }
        expect(unknown_threw, "unknown level is rejected");
    }

    // 7. 两个版本的阶段校验保持一致。
    expect(rejects_bad_phase(ai::Level::kRandom), "random AI rejects non-thinking phase");
    expect(rejects_bad_phase(ai::Level::kSearch), "search AI rejects non-thinking phase");

    if (failures == 0) {
        std::cout << "ALL PASS\n";
    } else {
        std::cout << failures << " FAILURE(S)\n";
    }
    return failures == 0 ? 0 : 1;
}
