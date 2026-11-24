// AI 搜索回归：最简 Expectiminimax 的合法性、战术与确定性。
//
// 覆盖：立即获胜、吃光对方、阻止对方下一步获胜、避免吃己方棋子、
// 降级选子、Botzone 式 current_side/player1_side 不一致、固定种子复现、
// 以及一整局双方都由 AI 走子时不产非法着法。

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

namespace {

int failures = 0;

void expect(bool cond, std::string_view what) {
    std::cout << (cond ? "PASS" : "FAIL") << ' ' << what << '\n';
    if (!cond) {
        ++failures;
    }
}

core::GameState base_state(core::Side current, int dice) {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = current;
    state.phase = (current == core::Side::Red) ? core::Phase::Player1Thinking
                                               : core::Phase::Player2Thinking;
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
        expect(move == expected, "AI takes the immediate winning move");
        expect(core::rules::check_move(state, move.from, move.to).empty(),
               "winning move is legal");
    }

    // 2. 吃光对方：红方吃掉蓝方最后一枚棋子。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {2, 2}, core::Side::Red, 1);
        put(state, {3, 3}, core::Side::Blue, 1);
        const core::Move move = choose(state, 2);
        const core::Move expected{{2, 2}, {3, 3}};
        expect(move == expected, "AI captures the last enemy piece");
    }

    // 3. 阻止对方下一步获胜：蓝方 (1,1) 下一手可进 (0,0)，红方必须吃掉它。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {1, 1}, core::Side::Blue, 1);
        const core::Move move = choose(state, 2);
        const core::Move expected{{0, 0}, {1, 1}};
        expect(move == expected, "AI blocks the opponent's winning move");
    }

    // 4. 避免吃己方棋子：存在安全着法时不应主动减少己方材料。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {1, 1}, core::Side::Red, 2);
        put(state, {4, 0}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        const core::Move move = choose(state, 2);
        const core::Pos own_target{1, 1};
        expect(!(move.to == own_target), "AI avoids capturing its own piece");
        expect(core::rules::check_move(state, move.from, move.to).empty(),
               "chosen non-capturing move is legal");
    }

    // 5. 降级选子：掷 3 且只有 1、6 号，只能从这两个编号出发。
    {
        core::GameState state = base_state(core::Side::Red, 3);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 6);
        put(state, {4, 0}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        const core::Move move = choose(state, 2);
        const int id = state.board.at(move.from).id;
        expect(id == 1 || id == 6, "AI only moves fallback-selectable pieces");
        expect(core::rules::check_move(state, move.from, move.to).empty(),
               "fallback move is legal");
    }

    // 6. Botzone 会把 player1_side 固定为红方；AI 必须按 current_side 行棋。
    {
        core::GameState state = base_state(core::Side::Blue, 1);
        state.player1_side = core::Side::Red;
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Red, 1);
        put(state, {0, 3}, core::Side::Red, 2);
        const core::Move move = choose(state, 2);
        const core::Pos blue_piece{4, 4};
        expect(move.from == blue_piece, "AI uses current_side rather than player1_side");
        expect(core::rules::check_move(state, move.from, move.to).empty(),
               "blue move is legal under current_side");
    }

    // 7. 固定种子 + 固定深度可复现。
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
        expect(first == second, "fixed seed and depth reproduce the same move");
    }

    // 8. 只有一个合法着法时直接返回（红方在 (4,3) 只能进 (4,4)）。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {4, 3}, core::Side::Red, 1);
        put(state, {0, 4}, core::Side::Blue, 1);
        const core::Move move = choose(state, 1);
        const core::Move expected{{4, 3}, {4, 4}};
        expect(move == expected, "single legal move is returned directly");
    }

    // 9. 非思考阶段拒绝；思考阶段但没有骰子则报告无路可走。
    {
        core::GameState idle;
        bool phase_threw = false;
        try {
            (void)ai::decide_move(idle);
        } catch (const std::invalid_argument&) {
            phase_threw = true;
        }
        expect(phase_threw, "AI rejects non-thinking phase");

        core::GameState no_dice = base_state(core::Side::Red, 1);
        no_dice.current_dice = std::nullopt;
        put(no_dice, {0, 0}, core::Side::Red, 1);
        put(no_dice, {4, 4}, core::Side::Blue, 1);
        bool no_move_threw = false;
        try {
            (void)ai::decide_move(no_dice);
        } catch (const std::runtime_error&) {
            no_move_threw = true;
        }
        expect(no_move_threw, "AI reports no legal move without dice");
    }

    // 10. 整局对弈：双方都由搜索 AI 走子，所有决策必须通过 core 规则校验。
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

        expect(okay, "AI plays a full game with only legal moves");
        expect(made_moves > 0, "AI actually made moves in the full game");
    }

    if (failures == 0) {
        std::cout << "ALL PASS\n";
    } else {
        std::cout << failures << " FAILURE(S)\n";
    }
    return failures == 0 ? 0 : 1;
}
