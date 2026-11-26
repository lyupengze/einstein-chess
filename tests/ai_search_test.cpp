/// @file tests/ai_search_test.cpp
/// @brief AI 搜索回归：竞速评估与 expectiminimax 的合法性、战术与确定性。
///
/// 覆盖立即获胜、吃光对方、阻止对方下一步获胜、竞速胜率与先手、吃己方棋子
/// 改善竞速、同分时推进/斜向优先、降级选子、Botzone 式 current_side 与
/// player1_side 不一致、固定种子复现，以及整局双方都由 AI 走子不产非法着法。

#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "ai/ai.h"
#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"
#include "test_support.h"

using test_support::expect;

namespace {

core::GameState base_state(core::Side current, int dice) {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = current;
    state.phase =
        (current == core::Side::Red) ? core::Phase::Player1Thinking : core::Phase::Player2Thinking;
    state.current_dice = dice;
    return state;
}

void put(core::GameState& state, core::Pos pos, core::Side side, int id) {
    state.board.at(pos) = {side, id};
}

core::Move choose(const core::GameState& state, int depth) {
    ai::SearchConfig config;
    config.max_depth = depth;
    return ai::decide_move(state, config);
}

}  // namespace

int main() {
    // 1. 立即获胜：红方 (3,3) 一步进 (4,4)。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {3, 3}, core::Side::Red, 1);
        put(state, {0, 4}, core::Side::Blue, 1);
        put(state, {1, 4}, core::Side::Blue, 2);
        const core::Move move = choose(state, 2);
        const core::Move expected{{3, 3}, {4, 4}};
        expect(move == expected, "立即获胜的着法被选中");
        expect(core::rules::check_move(state, move.from, move.to).empty(), "获胜着法合法");
    }

    // 2. 吃光对方：红方吃掉蓝方最后一枚棋子。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {2, 2}, core::Side::Red, 1);
        put(state, {3, 3}, core::Side::Blue, 1);
        const core::Move move = choose(state, 2);
        const core::Move expected{{2, 2}, {3, 3}};
        expect(move == expected, "吃光对方最后一枚棋子");
    }

    // 3. 阻止对方下一步获胜：蓝方 (1,1) 下一手可进 (0,0)，红方必须吃掉它。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {1, 1}, core::Side::Blue, 1);
        const core::Move move = choose(state, 2);
        const core::Move expected{{0, 0}, {1, 1}};
        expect(move == expected, "挡住对方下一步获胜");
    }

    // 4. 竞速评估：单子时 T 就是到目标角的距离，先手在等距时获胜。
    {
        core::GameState red_fast = base_state(core::Side::Red, 1);
        put(red_fast, {3, 4}, core::Side::Red, 1);   // 红 d1
        put(red_fast, {4, 0}, core::Side::Blue, 1);  // 蓝 d4
        expect(std::abs(ai::estimate_win_rate(red_fast) - 1.0) < 1e-6, "竞速：更近的一方胜");

        core::GameState red_slow = base_state(core::Side::Red, 1);
        put(red_slow, {1, 4}, core::Side::Red, 1);   // 红 d3
        put(red_slow, {2, 0}, core::Side::Blue, 1);  // 蓝 d2
        expect(std::abs(ai::estimate_win_rate(red_slow) - 0.0) < 1e-6, "竞速：更远的一方负");

        core::GameState equal = base_state(core::Side::Red, 1);
        put(equal, {2, 4}, core::Side::Red, 1);   // 红 d2
        put(equal, {2, 0}, core::Side::Blue, 1);  // 蓝 d2
        expect(std::abs(ai::estimate_win_rate(equal) - 1.0) < 1e-6, "竞速：等距时先手胜");

        core::GameState blue_to_move = base_state(core::Side::Blue, 1);
        put(blue_to_move, {3, 4}, core::Side::Red, 1);   // 红 d1
        put(blue_to_move, {4, 0}, core::Side::Blue, 1);  // 蓝 d4
        expect(std::abs(ai::estimate_win_rate(blue_to_move) - 0.0) < 1e-6,
               "竞速：胜率跟随当前行棋方");
    }

    // 5. 送子：吃掉己方更靠前的 2 号，让 1 号直接进入 d1；竞速模型不扣"材料分"。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {2, 2}, core::Side::Red, 1);   // 红 1 号 d2
        put(state, {3, 3}, core::Side::Red, 2);   // 红 2 号 d1
        put(state, {2, 0}, core::Side::Blue, 1);  // 蓝 d2
        put(state, {0, 2}, core::Side::Blue, 2);  // 蓝 d2
        const core::Move move = choose(state, 2);
        const core::Move expected{{2, 2}, {3, 3}};
        expect(move == expected, "可以吃己方棋子来改善竞速");
    }

    // 5b. 同分时优先推进：蓝 (3,3) d3、红 (4,0) d4，dice1；depth4 三个根着法
    //     都同分，但 (2,3)/(3,2) 不推进。先按走完距离过滤，再优先斜向。
    {
        core::GameState state = base_state(core::Side::Blue, 1);
        put(state, {3, 3}, core::Side::Blue, 1);
        put(state, {4, 0}, core::Side::Red, 1);
        ai::set_random_seed(123);
        const core::Move move = choose(state, 4);
        expect(move.to.x == 2 && move.to.y == 2, "同分时先推进再斜向");
    }

    // 6. 降级选子：掷 3 且只有 1、6 号，只能从这两个编号出发。
    {
        core::GameState state = base_state(core::Side::Red, 3);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 6);
        put(state, {4, 0}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        const core::Move move = choose(state, 2);
        const int id = state.board.at(move.from).id;
        expect(id == 1 || id == 6, "只动降级选子范围内的棋子");
        expect(core::rules::check_move(state, move.from, move.to).empty(), "降级选子的着法合法");
    }

    // 7. Botzone 会把 player1_side 固定为红方；AI 必须按 current_side 行棋。
    {
        core::GameState state = base_state(core::Side::Blue, 1);
        state.player1_side = core::Side::Red;
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Red, 1);
        put(state, {0, 3}, core::Side::Red, 2);
        const core::Move move = choose(state, 2);
        const core::Pos blue_piece{4, 4};
        expect(move.from == blue_piece, "按 current_side 而非 player1_side 行棋");
        expect(core::rules::check_move(state, move.from, move.to).empty(),
               "蓝方着法在 current_side 下合法");
    }

    // 8. 固定种子 + 固定深度可复现。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 2);
        put(state, {2, 2}, core::Side::Red, 6);
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {4, 3}, core::Side::Blue, 2);
        put(state, {2, 4}, core::Side::Blue, 6);
        ai::set_random_seed(2026);
        const core::Move first = choose(state, 3);
        ai::set_random_seed(2026);
        const core::Move second = choose(state, 3);
        expect(first == second, "固定种子与深度复现同一着法");
    }

    // 9. 只有一个合法着法时直接返回（红方在 (4,3) 只能进 (4,4)）。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {4, 3}, core::Side::Red, 1);
        put(state, {0, 4}, core::Side::Blue, 1);
        const core::Move move = choose(state, 1);
        const core::Move expected{{4, 3}, {4, 4}};
        expect(move == expected, "唯一合法着法直接返回");
    }

    // 10. 非思考阶段拒绝；思考阶段但没有骰子则报告无路可走。
    {
        core::GameState idle;
        bool phase_threw = false;
        try {
            [[maybe_unused]] const core::Move ignored = ai::decide_move(idle);
        } catch (const std::invalid_argument&) {
            phase_threw = true;
        }
        expect(phase_threw, "非思考阶段被拒绝");

        core::GameState no_dice = base_state(core::Side::Red, 1);
        no_dice.current_dice = std::nullopt;
        put(no_dice, {0, 0}, core::Side::Red, 1);
        put(no_dice, {4, 4}, core::Side::Blue, 1);
        bool no_move_threw = false;
        try {
            [[maybe_unused]] const core::Move ignored = ai::decide_move(no_dice);
        } catch (const std::runtime_error&) {
            no_move_threw = true;
        }
        expect(no_move_threw, "缺骰子时报无路可走");
    }

    // 11. 整局对弈：双方都由搜索 AI 走子，所有决策必须通过 core 规则校验。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});

        ai::SearchConfig config;
        config.max_depth = 2;
        bool okay = true;
        int made_moves = 0;

        for (int turn = 0; turn < 80; ++turn) {
            const core::GameState& state = game.get_state();
            if (state.phase == core::Phase::GameOver) {
                break;
            }
            if (state.phase == core::Phase::ThrowingDice) {
                game.roll_dice();
                continue;
            }
            if (state.phase != core::Phase::Player1Thinking &&
                state.phase != core::Phase::Player2Thinking) {
                okay = false;
                break;
            }

            try {
                const core::Move move = ai::decide_move(state, config);
                if (!core::rules::check_move(state, move.from, move.to).empty()) {
                    okay = false;
                    break;
                }
                game.make_move(move.from, move.to);
                ++made_moves;
            } catch (const std::exception&) {
                okay = false;
                break;
            }
        }

        expect(okay, "整局对弈只产生合法着法");
        expect(made_moves > 0, "整局确实走了若干步");
    }

    return test_support::finish();
}
