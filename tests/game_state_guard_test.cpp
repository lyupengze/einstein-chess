/// @file tests/game_state_guard_test.cpp
/// @brief Game 与 Board 的运行期守卫：非法调用必须抛 invalid_argument，且失败后状态不变。
///
/// 存档边界由 state_validation_test.cpp 覆盖；这里覆盖调用方绕开界面、直接驱动
/// core 的路径。每条断言都配一次状态快照比较，避免“抛了异常但状态已被改坏”。

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/types.h"
#include "test_support.h"

using test_support::expect;

namespace {

/// @brief 逐字段比较两个局面，包含棋盘快照。
bool same_state(const core::GameState& a, const core::GameState& b) {
    return a.player1_side == b.player1_side && a.current_side == b.current_side &&
           a.phase == b.phase && a.current_dice == b.current_dice && a.winner == b.winner &&
           a.player1_ready == b.player1_ready && a.player2_ready == b.player2_ready &&
           a.board.to_string() == b.board.to_string();
}

/// @brief 断言调用抛出 std::invalid_argument。
template <typename Fn>
bool throws_invalid(Fn&& fn) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

/// @brief 构造一个校验可通过的 GameOver 局面，用于验证“终局后不能再提交/走子”。
core::GameState make_game_over_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::None;
    state.phase = core::Phase::GameOver;
    state.winner = core::Side::Red;
    state.player1_ready = true;
    state.player2_ready = true;
    state.board.at({4, 4}) = {core::Side::Red, 1};
    state.board.at({0, 4}) = {core::Side::Blue, 1};
    return state;
}

}  // namespace

int main() {
    // 1. prepare_new_game 拒绝 None / 未知阵营，且失败不破坏旧局。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.prepare_new_game(core::Side::None); }),
               "prepare_new_game 拒绝 None");
        expect(same_state(game.get_state(), before), "被拒绝的 prepare_new_game 不改动旧局");

        expect(throws_invalid([&] { game.prepare_new_game(static_cast<core::Side>(99)); }),
               "prepare_new_game 拒绝未知阵营");
        expect(same_state(game.get_state(), before), "未知阵营的 prepare_new_game 不改动旧局");
    }

    // 2. submit_layout：同侧重复提交被拒，且不改变已提交的布阵。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::Red, {6, 5, 4, 3, 2, 1}); }),
               "重复提交布阵被拒绝");
        expect(same_state(game.get_state(), before), "被拒绝的重复布阵不改动棋盘");
    }

    // 3. submit_layout：非布阵阶段（ThrowingDice / GameOver）被拒。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::Blue, {6, 5, 4, 3, 2, 1}); }),
               "开赛后再提交布阵被拒绝");
        expect(same_state(game.get_state(), before), "开赛后提交布阵失败不改动状态");
    }
    {
        core::Game game;
        game.load_game(make_game_over_state());
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6}); }),
               "submit_layout 不能复活终局");
        expect(same_state(game.get_state(), before), "终局提交布阵失败不改动状态");
    }

    // 4. submit_layout：非法阵营 / 非法 layout 不改变状态。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::None, {1, 2, 3, 4, 5, 6}); }),
               "submit_layout 拒绝 None");
        expect(throws_invalid([&] { game.submit_layout(core::Side::Red, {1, 1, 2, 3, 4, 5}); }),
               "submit_layout 拒绝重复编号");
        expect(same_state(game.get_state(), before), "submit_layout 失败不改动状态");
    }

    // 5. Board::setup_custom 是“替换”而不是“叠加”：旧子（包括初始区外）被清掉。
    {
        core::Board board;
        board.at({4, 4}) = {core::Side::Red, 6};  // 模拟上一轮遗留在初始区外的旧子
        board.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});

        expect(board.count_pieces(core::Side::Red) == 6, "setup_custom 清掉初始区外的残留红子");
        expect(board.at({4, 4}).side == core::Side::None, "初始区外的残留红子被移除");

        // 再提交一次不同排列，仍然是 6 子、编号 1..6，不叠加。
        board.setup_custom(core::Side::Red, {6, 5, 4, 3, 2, 1});
        expect(board.count_pieces(core::Side::Red) == 6, "再次布阵后红方恰好 6 子");
        bool ids_ok = true;
        for (int id = 1; id <= core::Board::kChessCount; ++id) {
            ids_ok = ids_ok && board.has_piece(core::Side::Red, id);
        }
        expect(ids_ok, "再次布阵后编号 1~6 各一枚");
    }

    // 6. make_move：LayOut / ThrowingDice / GameOver 阶段都拒绝走子。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        expect(throws_invalid([&] { game.make_move({0, 0}, {0, 1}); }), "布阵阶段拒绝走子");

        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        expect(throws_invalid([&] { game.make_move({0, 0}, {0, 1}); }), "掷骰阶段拒绝走子");

        core::Game over;
        over.load_game(make_game_over_state());
        expect(throws_invalid([&] { over.make_move({4, 4}, {3, 4}); }), "终局拒绝走子");
    }

    // 7. roll_dice：LayOut 阶段被拒，且不改状态；正常 ThrowingDice 仍可掷。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.roll_dice(); }), "布阵阶段拒绝掷骰");
        expect(same_state(game.get_state(), before), "被拒绝的掷骰不改动状态");

        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        game.roll_dice();
        expect(game.get_state().current_dice.has_value(), "掷骰阶段仍可掷骰");
    }

    return test_support::finish();
}
