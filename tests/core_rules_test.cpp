// core 规则回归：合法着法枚举与走子结算。
//
// 这两件事原先散落在 AI 与 Game::make_move 里；下沉到 core 后必须保证：
// 1. generate_legal_moves 与"遍历 8 邻域 + check_move"的暴力结果完全一致；
// 2. apply_move_and_settle 与 Game::make_move 的结算结果完全一致；
// 3. 降级选子、吃己方棋子、抵达目标角、吃光对方等规则不回归。

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"
#include "test_support.h"

using test_support::expect;

namespace {

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
                    const core::Pos to{static_cast<std::size_t>(nx), static_cast<std::size_t>(ny)};
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
    state.phase =
        (current == core::Side::Red) ? core::Phase::Player1Thinking : core::Phase::Player2Thinking;
    state.current_dice = dice;
    return state;
}

void put(core::GameState& state, core::Pos pos, core::Side side, int id) {
    state.board.at(pos) = {side, id};
}

void expect_movegen_matches(const core::GameState& state, std::string_view what) {
    std::vector<core::Move> generated = core::rules::generate_legal_moves(state);
    std::vector<core::Move> brute = brute_force_moves(state);
    sort_moves(generated);
    sort_moves(brute);
    expect(generated == brute, what);
}

bool same_state(const core::GameState& a, const core::GameState& b) {
    return a.board.to_string() == b.board.to_string() && a.player1_side == b.player1_side &&
           a.current_side == b.current_side && a.phase == b.phase &&
           a.current_dice == b.current_dice && a.winner == b.winner &&
           a.player1_ready == b.player1_ready && a.player2_ready == b.player2_ready;
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
            const core::GameState expected = core::apply_move_and_settle(state, move.from, move.to);
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
        const core::GameState next = core::apply_move_and_settle(red_win, {3, 3}, {4, 4});
        expect(next.winner == core::Side::Red && next.phase == core::Phase::GameOver &&
                   !next.current_dice,
               "red reaching (4,4) wins and clears dice");

        core::GameState blue_win = base_state(core::Side::Blue, 1);
        put(blue_win, {1, 1}, core::Side::Blue, 1);
        put(blue_win, {0, 4}, core::Side::Red, 1);
        const core::GameState next_blue = core::apply_move_and_settle(blue_win, {1, 1}, {0, 0});
        expect(next_blue.winner == core::Side::Blue && next_blue.phase == core::Phase::GameOver,
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

    // 9. 降级选子的完整表：同号优先、只有下界、只有上界、上下界并存、无子可动、骰子越界。
    {
        auto selectable = [](int dice, std::vector<int> alive) {
            return core::rules::selectable_ids_for(dice, [&alive](int id) {
                return std::find(alive.begin(), alive.end(), id) != alive.end();
            });
        };
        expect(selectable(3, {2, 3, 4}) == std::array<int, 2>{3, -1},
               "selectable ids prefer the exact dice id");
        expect(selectable(3, {1, 2}) == std::array<int, 2>{2, -1},
               "selectable ids fall back to the nearest id below the dice");
        expect(selectable(3, {4, 5}) == std::array<int, 2>{-1, 4},
               "selectable ids fall back to the nearest id above the dice");
        expect(selectable(3, {1, 6}) == std::array<int, 2>{1, 6},
               "selectable ids keep both fallback neighbours");
        expect(selectable(3, std::vector<int>{}) == std::array<int, 2>{-1, -1},
               "no alive piece yields no selectable id");
        expect(selectable(0, {1, 2, 3}) == std::array<int, 2>{-1, -1} &&
                   selectable(7, {1, 2, 3}) == std::array<int, 2>{-1, -1},
               "out-of-range dice yields no selectable id");

        core::GameState exact = base_state(core::Side::Red, 3);
        put(exact, {0, 0}, core::Side::Red, 3);
        put(exact, {0, 1}, core::Side::Red, 1);
        expect(core::rules::get_selectable_ids(exact) == std::vector<int>{3},
               "get_selectable_ids keeps only the exact dice piece");
    }

    // 10. check_move 的拒绝顺序与文案：越界 -> 起点归属 -> 是否可选 -> 方向。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {2, 2}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        expect(core::rules::check_move(state, {5, 0}, {4, 0}) == "坐标超出棋盘范围",
               "check_move rejects an out-of-bounds start");
        expect(core::rules::check_move(state, {2, 2}, {5, 0}) == "坐标超出棋盘范围",
               "check_move rejects an out-of-bounds destination");
        expect(core::rules::check_move(state, {4, 4}, {3, 3}) == "起点不是己方棋子",
               "check_move reports the wrong starting side");
        expect(core::rules::check_move(state, {2, 2}, {3, 2}).empty(), "red forward step is legal");
        expect(core::rules::check_move(state, {2, 2}, {1, 2}) == "红方只能向下、向右、向右下移动",
               "red cannot step backwards");
        expect(core::rules::check_move(state, {2, 2}, {2, 2}) == "红方只能向下、向右、向右下移动",
               "red cannot stay in place");
    }
    {
        core::GameState state = base_state(core::Side::Blue, 1);
        put(state, {2, 2}, core::Side::Blue, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        expect(core::rules::check_move(state, {2, 2}, {1, 2}).empty(),
               "blue forward step is legal");
        expect(core::rules::check_move(state, {2, 2}, {3, 2}) == "蓝方只能向上、向左、向左上移动",
               "blue cannot step backwards");
    }
    {
        // 骰 3 时场上只有 1、2 号：下界 2 可选，1 号不可选。
        core::GameState state = base_state(core::Side::Red, 3);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 2);
        put(state, {4, 4}, core::Side::Blue, 1);
        expect(core::rules::check_move(state, {0, 0}, {1, 0}) == "该棋子不可被移动",
               "check_move rejects a piece outside the fallback selection");
        expect(core::rules::check_move(state, {0, 1}, {1, 1}).empty(),
               "check_move accepts the fallback-selected piece");
    }

    // 11. 胜负只看目标角：红方在左上角、蓝方在右下角都不算赢。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        expect(core::rules::check_winner(state) == core::Side::None,
               "pieces on the wrong corners do not win");
        put(state, {4, 4}, core::Side::Red, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        expect(core::rules::check_winner(state) == core::Side::Red, "red wins by occupying (4,4)");
    }
    {
        core::GameState state = base_state(core::Side::Blue, 1);
        put(state, {0, 0}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Red, 1);
        expect(core::rules::check_winner(state) == core::Side::Blue,
               "blue wins by occupying (0,0)");
    }

    // 12. 未分胜负的结算：换手、回到掷骰、清空本回合骰子。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        const core::GameState next = core::apply_move_and_settle(state, {0, 0}, {0, 1});
        expect(next.current_side == core::Side::Blue && next.phase == core::Phase::ThrowingDice &&
                   !next.current_dice && next.winner == core::Side::None,
               "non-winning move switches side, returns to dice, clears dice");
    }

    // 13. 布阵落位顺序：红方行优先展开，蓝方从右下角镜像展开。
    {
        core::Board red;
        red.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});
        expect(red.at({0, 0}).id == 1 && red.at({0, 1}).id == 2 && red.at({0, 2}).id == 3 &&
                   red.at({1, 0}).id == 4 && red.at({1, 1}).id == 5 && red.at({2, 0}).id == 6,
               "red layout follows the row-major region order");
        expect(red.count_pieces(core::Side::Red) == 6, "red layout places six pieces");

        core::Board blue;
        blue.setup_custom(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        expect(blue.at({4, 4}).id == 1 && blue.at({4, 3}).id == 2 && blue.at({4, 2}).id == 3 &&
                   blue.at({3, 4}).id == 4 && blue.at({3, 3}).id == 5 && blue.at({2, 4}).id == 6,
               "blue layout mirrors the red region order");

        core::Board untouched;
        untouched.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});
        const auto before = untouched.to_string();
        bool duplicate_threw = false;
        try {
            untouched.setup_custom(core::Side::Red, {1, 1, 2, 3, 4, 5});
        } catch (const std::invalid_argument&) {
            duplicate_threw = true;
        }
        expect(duplicate_threw, "duplicate layout id is rejected");
        expect(untouched.to_string() == before, "invalid layout leaves the board unchanged");

        bool none_threw = false;
        try {
            untouched.setup_custom(core::Side::None, {1, 2, 3, 4, 5, 6});
        } catch (const std::invalid_argument&) {
            none_threw = true;
        }
        expect(none_threw, "Side::None cannot submit a layout");
        expect(untouched.to_string() == before, "rejected side leaves the board unchanged");
    }

    // 14. Game 的布阵生命周期与掷骰入口。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Blue);
        core::GameState state = game.get_state();
        expect(state.phase == core::Phase::LayOut && state.player1_side == core::Side::Blue &&
                   state.board.count_pieces(core::Side::Red) == 0,
               "prepare_new_game resets to an empty LayOut state");

        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        state = game.get_state();
        expect(state.player1_ready && !state.player2_ready && state.phase == core::Phase::LayOut,
               "one submitted layout keeps the game in LayOut");

        game.submit_layout(core::Side::Red, {6, 5, 4, 3, 2, 1});
        state = game.get_state();
        expect(state.player1_ready && state.player2_ready &&
                   state.phase == core::Phase::ThrowingDice &&
                   state.current_side == core::Side::Red,
               "both layouts start the game with Red to throw");

        game.roll_dice();
        state = game.get_state();
        expect(state.current_dice && *state.current_dice >= 1 && *state.current_dice <= 6,
               "roll_dice produces a value in 1..6");
        expect(state.phase == core::Phase::Player2Thinking,
               "player1 is Blue, so Red's throw is Player2Thinking");
    }
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        bool roll_threw = false;
        try {
            game.roll_dice();
        } catch (const std::invalid_argument&) {
            roll_threw = true;
        }
        expect(roll_threw, "roll_dice rejects the LayOut phase");

        bool layout_threw = false;
        try {
            game.submit_layout(core::Side::Red, {1, 1, 2, 3, 4, 5});
        } catch (const std::invalid_argument&) {
            layout_threw = true;
        }
        expect(layout_threw && !game.get_state().player1_ready &&
                   game.get_state().phase == core::Phase::LayOut,
               "invalid layout does not mark the side ready");
    }

    return test_support::finish();
}
