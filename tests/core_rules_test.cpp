/// @file tests/core_rules_test.cpp
/// @brief core 规则回归：合法着法枚举、走子结算与布阵生命周期。
///
/// 固定三条契约：
/// 1. generate_legal_moves 与“遍历 8 邻域 + check_move”的暴力结果完全一致；
/// 2. apply_move_and_settle 与 Game::make_move 的结算结果完全一致；
/// 3. 降级选子、吃己方棋子、抵达目标角、吃光对方等规则保持不变。

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
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
            expect_movegen_matches(red, "红方着法枚举与暴力扫描一致");
        }

        core::GameState blue = base_state(core::Side::Blue, 1);
        put(blue, {4, 4}, core::Side::Blue, 1);
        put(blue, {3, 4}, core::Side::Blue, 2);
        put(blue, {4, 3}, core::Side::Blue, 6);
        put(blue, {1, 1}, core::Side::Red, 4);
        put(blue, {2, 2}, core::Side::Red, 5);
        for (int dice = 1; dice <= 6; ++dice) {
            blue.current_dice = dice;
            expect_movegen_matches(blue, "蓝方着法枚举与暴力扫描一致");
        }
    }

    // 2. 未掷骰 / 行棋方为 None 时没有合法着法。
    {
        core::GameState no_dice = base_state(core::Side::Red, 1);
        no_dice.current_dice = std::nullopt;
        put(no_dice, {0, 0}, core::Side::Red, 1);
        expect(core::rules::generate_legal_moves(no_dice).empty(), "无骰子时没有合法着法");

        core::GameState no_side = base_state(core::Side::None, 1);
        put(no_side, {0, 0}, core::Side::Red, 1);
        expect(core::rules::generate_legal_moves(no_side).empty(), "行棋方为 None 时没有合法着法");
    }

    // 3. 降级选子：掷 3，红方场上只有 1、6 号，两个编号都可动。
    {
        core::GameState state = base_state(core::Side::Red, 3);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 6);
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        const auto ids = core::rules::get_selectable_ids(state);
        expect(ids.size() == 2 && ids[0] == 1 && ids[1] == 6, "降级选子取骰点上下最近的编号");
        expect_movegen_matches(state, "降级选子的着法枚举与暴力扫描一致");
    }

    // 4. 吃己方棋子仍然合法（规则如此），落点覆盖后己方棋子数减少。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {1, 1}, core::Side::Red, 2);
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        expect(core::rules::check_move(state, {0, 0}, {1, 1}).empty(), "吃己方棋子合法");
        core::rules::apply_move(state, {0, 0}, {1, 1});
        expect(state.board.count_pieces(core::Side::Red) == 1, "吃己方棋子后目标子消失");
    }

    // 5. 结算等价：apply_move_and_settle == Game::make_move。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 2);
        put(state, {4, 4}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Blue, 2);

        const std::vector<core::Move> moves = core::rules::generate_legal_moves(state);
        expect(!moves.empty(), "结算用例存在合法着法");
        for (const core::Move& move : moves) {
            const core::GameState expected = core::apply_move_and_settle(state, move.from, move.to);
            core::Game game;
            game.load_game(state);
            game.make_move(move.from, move.to);
            expect(same_state(expected, game.get_state()),
                   "apply_move_and_settle 与 Game::make_move 一致");
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
               "红方抵达 (4,4) 判胜并清空骰子");

        core::GameState blue_win = base_state(core::Side::Blue, 1);
        put(blue_win, {1, 1}, core::Side::Blue, 1);
        put(blue_win, {0, 4}, core::Side::Red, 1);
        const core::GameState next_blue = core::apply_move_and_settle(blue_win, {1, 1}, {0, 0});
        expect(next_blue.winner == core::Side::Blue && next_blue.phase == core::Phase::GameOver,
               "蓝方抵达 (0,0) 判胜");
    }

    // 7. 吃光对方判胜。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {2, 2}, core::Side::Red, 1);
        put(state, {3, 3}, core::Side::Blue, 1);
        const core::GameState next = core::apply_move_and_settle(state, {2, 2}, {3, 3});
        expect(next.winner == core::Side::Red && next.phase == core::Phase::GameOver,
               "吃光对方最后一子判胜");
    }

    // 8. 非法走法抛异常，且原状态不被修改。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        const core::GameState before = state;
        bool threw = false;
        try {
            [[maybe_unused]] const auto& ignored =
                core::apply_move_and_settle(state, {0, 0}, {1, 2});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        expect(threw, "非法走法抛 std::invalid_argument");
        expect(same_state(state, before), "非法走法不修改原局面");
    }

    // 9. 降级选子的完整表：同号优先、只有下界、只有上界、上下界并存、无子可动、骰子越界。
    {
        auto selectable = [](int dice, std::vector<int> alive) {
            return core::rules::selectable_ids_for(dice, [&alive](int id) {
                return std::find(alive.begin(), alive.end(), id) != alive.end();
            });
        };
        expect(selectable(3, {2, 3, 4}) == std::array<int, 2>{3, -1}, "有同号棋子时只选该编号");
        expect(selectable(3, {1, 2}) == std::array<int, 2>{2, -1}, "无同号时取小于骰点的最近编号");
        expect(selectable(3, {4, 5}) == std::array<int, 2>{-1, 4}, "无同号时取大于骰点的最近编号");
        expect(selectable(3, {1, 6}) == std::array<int, 2>{1, 6}, "上下界并存时两个编号都保留");
        expect(selectable(3, std::vector<int>{}) == std::array<int, 2>{-1, -1},
               "没有存活棋子时没有可选编号");
        expect(selectable(0, {1, 2, 3}) == std::array<int, 2>{-1, -1} &&
                   selectable(7, {1, 2, 3}) == std::array<int, 2>{-1, -1},
               "骰点越界时没有可选编号");

        core::GameState exact = base_state(core::Side::Red, 3);
        put(exact, {0, 0}, core::Side::Red, 3);
        put(exact, {0, 1}, core::Side::Red, 1);
        expect(core::rules::get_selectable_ids(exact) == std::vector<int>{3},
               "get_selectable_ids 只保留同号棋子");
    }

    // 10. check_move 的拒绝文案：越界、起点归属、是否可选、方向。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {2, 2}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        expect(core::rules::check_move(state, {5, 0}, {4, 0}) == "坐标超出棋盘范围",
               "check_move 拒绝越界起点");
        expect(core::rules::check_move(state, {2, 2}, {5, 0}) == "坐标超出棋盘范围",
               "check_move 拒绝越界终点");
        expect(core::rules::check_move(state, {4, 4}, {3, 3}) == "起点不是己方棋子",
               "check_move 拒绝非己方起点");
        expect(core::rules::check_move(state, {2, 2}, {3, 2}).empty(), "红方前进合法");
        expect(core::rules::check_move(state, {2, 2}, {1, 2}) == "红方只能向下、向右、向右下移动",
               "红方不能后退");
        expect(core::rules::check_move(state, {2, 2}, {2, 2}) == "红方只能向下、向右、向右下移动",
               "红方不能原地不动");
    }
    {
        core::GameState state = base_state(core::Side::Blue, 1);
        put(state, {2, 2}, core::Side::Blue, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        expect(core::rules::check_move(state, {2, 2}, {1, 2}).empty(), "蓝方前进合法");
        expect(core::rules::check_move(state, {2, 2}, {3, 2}) == "蓝方只能向上、向左、向左上移动",
               "蓝方不能后退");
    }
    {
        // 骰 3 时场上只有 1、2 号：下界 2 可选，1 号不可选。
        core::GameState state = base_state(core::Side::Red, 3);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {0, 1}, core::Side::Red, 2);
        put(state, {4, 4}, core::Side::Blue, 1);
        expect(core::rules::check_move(state, {0, 0}, {1, 0}) == "该棋子不可被移动",
               "check_move 拒绝降级选子之外的棋子");
        expect(core::rules::check_move(state, {0, 1}, {1, 1}).empty(),
               "check_move 接受降级选中的棋子");
    }

    // 11. 胜负只看目标角：红方在左上角、蓝方在右下角都不算赢。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        expect(core::rules::check_winner(state) == core::Side::None, "棋子不在目标角不算胜");
        put(state, {4, 4}, core::Side::Red, 1);
        put(state, {0, 4}, core::Side::Blue, 2);
        expect(core::rules::check_winner(state) == core::Side::Red, "红方占据 (4,4) 判胜");
    }
    {
        core::GameState state = base_state(core::Side::Blue, 1);
        put(state, {0, 0}, core::Side::Blue, 1);
        put(state, {0, 4}, core::Side::Red, 1);
        expect(core::rules::check_winner(state) == core::Side::Blue, "蓝方占据 (0,0) 判胜");
    }

    // 12. 未分胜负的结算：换手、回到掷骰、清空本回合骰子。
    {
        core::GameState state = base_state(core::Side::Red, 1);
        put(state, {0, 0}, core::Side::Red, 1);
        put(state, {4, 4}, core::Side::Blue, 1);
        const core::GameState next = core::apply_move_and_settle(state, {0, 0}, {0, 1});
        expect(next.current_side == core::Side::Blue && next.phase == core::Phase::ThrowingDice &&
                   !next.current_dice && next.winner == core::Side::None,
               "未分胜负时换手、回到掷骰并清空骰子");
    }

    // 13. 布阵落位顺序：红方行优先展开，蓝方从右下角镜像展开；坐标表与反查下标一致。
    {
        core::Board red;
        red.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});
        expect(red.at({0, 0}).id == 1 && red.at({0, 1}).id == 2 && red.at({0, 2}).id == 3 &&
                   red.at({1, 0}).id == 4 && red.at({1, 1}).id == 5 && red.at({2, 0}).id == 6,
               "红方按初始区行优先顺序落位");
        expect(red.count_pieces(core::Side::Red) == 6, "红方布阵落下 6 枚棋子");

        core::Board blue;
        blue.setup_custom(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        expect(blue.at({4, 4}).id == 1 && blue.at({4, 3}).id == 2 && blue.at({4, 2}).id == 3 &&
                   blue.at({3, 4}).id == 4 && blue.at({3, 3}).id == 5 && blue.at({2, 4}).id == 6,
               "蓝方布阵按红方初始区镜像落位");

        // 坐标表是初始区的唯一事实源：落位顺序、界面反查下标都从它派生。
        expect(
            core::Board::start_positions(core::Side::Red) == core::Board::kRedStartPositions &&
                core::Board::start_positions(core::Side::Blue) == core::Board::kBlueStartPositions,
            "start_positions 返回对应阵营的坐标表");
        expect(core::Board::kRedStartPositions.front() == core::Pos{0, 0} &&
                   core::Board::kRedStartPositions.back() == core::Pos{2, 0} &&
                   core::Board::kBlueStartPositions.front() == core::Pos{4, 4} &&
                   core::Board::kBlueStartPositions.back() == core::Pos{2, 4},
               "坐标表首尾元素与初始区形状一致");

        const auto& red_positions = core::Board::start_positions(core::Side::Red);
        bool layout_matches_positions = true;
        for (int i = 0; i < core::Board::kChessCount; ++i) {
            const core::Pos pos = red_positions[static_cast<std::size_t>(i)];
            layout_matches_positions = layout_matches_positions && red.at(pos).id == i + 1 &&
                                       core::Board::start_index(core::Side::Red, pos) == i;
        }
        expect(layout_matches_positions, "布阵落位顺序与坐标表一致");

        expect(core::Board::start_index(core::Side::Red, {4, 4}) == std::nullopt &&
                   core::Board::start_index(core::Side::Blue, {0, 0}) == std::nullopt &&
                   core::Board::start_index(core::Side::Red, {5, 0}) == std::nullopt,
               "对方初始区与棋盘外不对应任何下标");

        bool positions_none_threw = false;
        try {
            (void)core::Board::start_positions(core::Side::None);
        } catch (const std::invalid_argument&) {
            positions_none_threw = true;
        }
        expect(positions_none_threw, "start_positions 拒绝 Side::None");

        core::Board untouched;
        untouched.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});
        const auto before = untouched.to_string();
        bool duplicate_threw = false;
        try {
            untouched.setup_custom(core::Side::Red, {1, 1, 2, 3, 4, 5});
        } catch (const std::invalid_argument&) {
            duplicate_threw = true;
        }
        expect(duplicate_threw, "重复编号的布阵被拒绝");
        expect(untouched.to_string() == before, "非法布阵不修改棋盘");

        bool none_threw = false;
        try {
            untouched.setup_custom(core::Side::None, {1, 2, 3, 4, 5, 6});
        } catch (const std::invalid_argument&) {
            none_threw = true;
        }
        expect(none_threw, "Side::None 不能提交布阵");
        expect(untouched.to_string() == before, "被拒绝的一方不修改棋盘");
    }

    // 14. Game 的布阵生命周期与掷骰入口。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Blue);
        core::GameState state = game.get_state();
        expect(state.phase == core::Phase::LayOut && state.player1_side == core::Side::Blue &&
                   state.board.count_pieces(core::Side::Red) == 0,
               "prepare_new_game 重置为空布阵局面");

        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        state = game.get_state();
        expect(state.player1_ready && !state.player2_ready && state.phase == core::Phase::LayOut,
               "只有一方就绪时仍停留在布阵阶段");

        game.submit_layout(core::Side::Red, {6, 5, 4, 3, 2, 1});
        state = game.get_state();
        expect(state.player1_ready && state.player2_ready &&
                   state.phase == core::Phase::ThrowingDice &&
                   state.current_side == core::Side::Red,
               "双方就绪后由红方先掷骰");

        game.roll_dice();
        state = game.get_state();
        expect(state.current_dice && *state.current_dice >= 1 && *state.current_dice <= 6,
               "掷骰结果为 1~6");
        expect(state.phase == core::Phase::Player2Thinking,
               "我方为蓝方时红方掷骰进入 Player2Thinking");
    }
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        bool roll_threw = false;
        try {
            game.roll_dice();
        } catch (const std::logic_error&) {
            roll_threw = true;
        }
        expect(roll_threw, "布阵阶段不能掷骰");

        bool layout_threw = false;
        try {
            game.submit_layout(core::Side::Red, {1, 1, 2, 3, 4, 5});
        } catch (const std::invalid_argument&) {
            layout_threw = true;
        }
        expect(layout_threw && !game.get_state().player1_ready &&
                   game.get_state().phase == core::Phase::LayOut,
               "非法布阵不置就绪标志");
    }

    // 15. Board::at 的越界异常必须带坐标，方便定位误用。
    {
        core::Board board;
        bool threw = false;
        std::string message;
        try {
            [[maybe_unused]] const core::Piece& ignored = board.at({5, 0});
        } catch (const std::out_of_range& error) {
            threw = true;
            message = error.what();
        }
        expect(threw, "越界访问棋盘抛 std::out_of_range");
        expect(message.find("(5, 0)") != std::string::npos, "越界异常信息包含坐标");
    }

    return test_support::finish();
}
