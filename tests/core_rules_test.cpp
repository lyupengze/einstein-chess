// core 规则回归：合法着法枚举与走子结算。
//
// 这两件事原先散落在 AI 与 Game::make_move 里；下沉到 core 后必须保证：
// 1. generate_legal_moves 与"遍历 8 邻域 + check_move"的暴力结果完全一致；
// 2. apply_move_and_settle 与 Game::make_move 的结算结果完全一致；
// 3. 降级选子、吃己方棋子、抵达目标角、吃光对方等规则不回归。

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <vector>

#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"

namespace {

int failures = 0;

void expect(bool cond, const char* what) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) {
        ++failures;
    }
}

bool move_less(const core::Move& a, const core::Move& b) {
    if (a.from.x != b.from.x) {
        return a.from.x < b.from.x;
    }
    if (a.from.y != b.from.y) {
        return a.from.y < b.from.y;
    }
    if (a.to.x != b.to.x) {
        return a.to.x < b.to.x;
    }
    return a.to.y < b.to.y;
}

void sort_moves(std::vector<core::Move>& moves) {
    std::sort(moves.begin(), moves.end(), move_less);
}

std::vector<core::Move> brute_force_moves(const core::GameState& state) {
    std::vector<core::Move> moves;
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const core::Pos from{x, y};
            if (state.board.at(from).side != state.current_side) {
                continue;
            }
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const int nx = static_cast<int>(x) + dx;
                    const int ny = static_cast<int>(y) + dy;
                    const core::Pos to{static_cast<std::size_t>(nx),
                                       static_cast<std::size_t>(ny)};
                    if (core::rules::check_move(state, from, to).empty()) {
                        moves.push_back({from, to});
                    }
                }
            }
        }
    }
    return moves;
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

void expect_movegen_matches(const core::GameState& state, const char* what) {
    std::vector<core::Move> generated = core::rules::generate_legal_moves(state);
    std::vector<core::Move> brute = brute_force_moves(state);
    sort_moves(generated);
    sort_moves(brute);
    expect(generated == brute, what);
}

bool same_state(const core::GameState& a, const core::GameState& b) {
    return a.board.to_string() == b.board.to_string() &&
           a.player1_side == b.player1_side && a.current_side == b.current_side &&
           a.phase == b.phase && a.current_dice == b.current_dice &&
           a.winner == b.winner && a.player1_ready == b.player1_ready &&
           a.player2_ready == b.player2_ready;
}

}  // namespace

int main() {
    // 1. 合法着法枚举与暴力扫描逐项一致（红方 / 蓝方、各个骰子点数）。
    {
        core::GameState red = base_state(core::Side::Red, 1);
        put(red, {0, 0}, core::Side::Red, 1);
        put(red, {0, 1}, core::Side::Red, 2);
        put(red, {3, 0}, core::Side::Red, 6);
        put(red, {4, 4}, core::Side::Blue, 1);
        put(red, {0, 4}, core::Side::Blue, 2);
        for (int dice = 1; dice <= 6; ++dice) {
            red.current_dice = dice;
            expect_movegen_matches(red, "red movegen matches brute force");
        }

        core::GameState blue = base_state(core::Side::Blue, 1);
        put(blue, {4, 4}, core::Side::Blue, 1);
        put(blue, {3, 4}, core::Side::Blue, 2);
        put(blue, {4, 3}, core::Side::Blue, 6);
        put(blue, {1, 1}, core::Side::Red, 4);
        put(blue, {2, 2}, core::Side::Red, 5);
        for (int dice = 1; dice <= 6; ++dice) {
            blue.current_dice = dice;
            expect_movegen_matches(blue, "blue movegen matches brute force");
        }
    }

    // 2. 未掷骰 / 行棋方为 None 时没有合法着法。
    {
        core::GameState no_dice = base_state(core::Side::Red, 1);
        no_dice.current_dice = std::nullopt;
        put(no_dice, {0, 0}, core::Side::Red, 1);
        expect(core::rules::generate_legal_moves(no_dice).empty(), "no dice yields no moves");

        core::GameState no_side = base_state(core::Side::None, 1);
        put(no_side, {0, 0}, core::Side::Red, 1);
        expect(core::rules::generate_legal_moves(no_side).empty(), "no side yields no moves");
    }

    // 3. 降级选子：掷 3 且场上只有 1、6 号，两个编号都可动。
    {
        core::GameState state = base_state(core::Side::Red, 3);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 6);
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        const auto ids = core::rules::get_selectable_ids(state);
        expect(ids.size() == 2 && ids[0] == 1 && ids[1] == 6,
               "fallback selects nearest ids below and above the dice");
        expect_movegen_matches(state, "fallback movegen matches brute force");
    }

    // 4. 吃己方棋子仍然合法（规则如此），落点覆盖后己方棋子数减少。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {1, 1}, core::Side::Red, 2);
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        expect(core::rules::check_move(state, {0, 0}, {1, 1}).empty(),
               "capturing own piece is legal");
        core::rules::apply_move(state, {0, 0}, {1, 1});
        expect(state.board.count_pieces(core::Side::Red) == 1,
               "capturing own piece removes the target");
    }

    // 5. 结算等价：apply_move_and_settle == Game::make_move。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 2);
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);

        const std::vector<core::Move> moves = core::rules::generate_legal_moves(state);
        expect(!moves.empty(), "settle fixture has legal moves");
        for (const core::Move& move : moves) {
            const core::GameState expected =
                core::apply_move_and_settle(state, move.from, move.to);
            core::Game game;
            game.load_game(state);
            game.make_move(move.from, move.to);
            expect(same_state(expected, game.get_state()),
                   "apply_move_and_settle matches Game::make_move");
        }
    }

    // 6. 抵达目标角判胜。
    {
        core::GameState red_win = base_state(core::Side::Red, 1);
        put(red_win, {3, 3}, core::Side::Red, 1);
        put(red_win, {0, 4}, core::Side::Blue, 1);
        const core::GameState next =
            core::apply_move_and_settle(red_win, {3, 3}, {4, 4});
        expect(next.winner == core::Side::Red && next.phase == core::Phase::GameOver &&
                   !next.current_dice,
               "red reaching (4,4) wins and clears dice");

        core::GameState blue_win = base_state(core::Side::Blue, 1);
        put(blue_win, {1, 1}, core::Side::Blue, 1);
        put(blue_win, {0, 4}, core::Side::Red, 1);
        const core::GameState next_blue =
            core::apply_move_and_settle(blue_win, {1, 1}, {0, 0});
        expect(next_blue.winner == core::Side::Blue &&
                   next_blue.phase == core::Phase::GameOver,
               "blue reaching (0,0) wins");
    }

    // 7. 吃光对方判胜。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {2, 2}, core::Side::Red, 1);
        put(state, {3, 3}, core::Side::Blue, 1);
        const core::GameState next = core::apply_move_and_settle(state, {2, 2}, {3, 3});
        expect(next.winner == core::Side::Red && next.phase == core::Phase::GameOver,
               "capturing the last enemy piece wins");
    }

    // 8. 非法走法抛异常，且原状态不被修改。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        const core::GameState before = state;
        bool threw = false;
        try {
            (void)core::apply_move_and_settle(state, {0, 0}, {1, 2});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        expect(threw, "illegal move throws std::invalid_argument");
        expect(same_state(state, before), "illegal move leaves the original state unchanged");
    }

    std::printf(failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
