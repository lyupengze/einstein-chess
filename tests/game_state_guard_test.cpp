// Game / Board 状态机守卫测试。
//
// P0-1 把“损坏存档”挡在了 load/save 边界；本文件覆盖运行期公共 API 的另一半：
// 即使调用方绕开界面直接驱动 core，也不能让 Game 进入非法状态。
// 重点不是“错误文案”，而是“非法调用必须抛异常，且失败后状态不变”。

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
               "prepare_new_game rejects Side::None");
        expect(same_state(game.get_state(), before), "rejected prepare_new_game keeps old state");

        expect(throws_invalid([&] {
                   game.prepare_new_game(static_cast<core::Side>(99));
               }),
               "prepare_new_game rejects unknown side");
        expect(same_state(game.get_state(), before),
               "unknown-side prepare_new_game keeps old state");
    }

    // 2. submit_layout：同侧重复提交被拒，且不改变已提交的布阵。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::Red, {6, 5, 4, 3, 2, 1}); }),
               "duplicate submit_layout is rejected");
        expect(same_state(game.get_state(), before),
               "rejected duplicate submit_layout keeps the board");
    }

    // 3. submit_layout：非布阵阶段（ThrowingDice / GameOver）被拒。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::Blue, {6, 5, 4, 3, 2, 1}); }),
               "submit_layout is rejected after the game starts");
        expect(same_state(game.get_state(), before),
               "rejected post-start submit_layout keeps the state");
    }
    {
        core::Game game;
        game.load_game(make_game_over_state());
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6}); }),
               "submit_layout cannot revive a GameOver state");
        expect(same_state(game.get_state(), before),
               "rejected GameOver submit_layout keeps the state");
    }

    // 4. submit_layout：非法阵营 / 非法 layout 不改变状态。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.submit_layout(core::Side::None, {1, 2, 3, 4, 5, 6}); }),
               "submit_layout rejects Side::None");
        expect(throws_invalid([&] { game.submit_layout(core::Side::Red, {1, 1, 2, 3, 4, 5}); }),
               "submit_layout rejects a duplicate layout id");
        expect(same_state(game.get_state(), before), "rejected submit_layout keeps the state");
    }

    // 5. Board::setup_custom 是“替换”而不是“叠加”：旧子（包括初始区外）被清掉。
    {
        core::Board board;
        board.at({4, 4}) = {core::Side::Red, 6};  // 模拟上一轮遗留在初始区外的旧子
        board.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});

        expect(board.count_pieces(core::Side::Red) == 6,
               "setup_custom clears stale red pieces outside the region");
        expect(board.at({4, 4}).side == core::Side::None,
               "stale red piece outside the initial region is removed");

        // 再提交一次不同排列，仍然是 6 子、编号 1..6，不叠加。
        board.setup_custom(core::Side::Red, {6, 5, 4, 3, 2, 1});
        expect(board.count_pieces(core::Side::Red) == 6,
               "repeated setup_custom keeps exactly six red pieces");
        bool ids_ok = true;
        for (int id = 1; id <= core::Board::kChessCount; ++id) {
            ids_ok = ids_ok && board.has_piece(core::Side::Red, id);
        }
        expect(ids_ok, "repeated setup_custom keeps ids 1..6 exactly once");
    }

    // 6. make_move：LayOut / ThrowingDice / GameOver 阶段都拒绝走子。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        expect(throws_invalid([&] { game.make_move({0, 0}, {0, 1}); }),
               "make_move is rejected in LayOut");

        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        expect(throws_invalid([&] { game.make_move({0, 0}, {0, 1}); }),
               "make_move is rejected in ThrowingDice");

        core::Game over;
        over.load_game(make_game_over_state());
        expect(throws_invalid([&] { over.make_move({4, 4}, {3, 4}); }),
               "make_move is rejected in GameOver");
    }

    // 7. roll_dice：LayOut 阶段被拒，且不改状态；正常 ThrowingDice 仍可掷。
    {
        core::Game game;
        game.prepare_new_game(core::Side::Red);
        const core::GameState before = game.get_state();

        expect(throws_invalid([&] { game.roll_dice(); }), "roll_dice rejects LayOut");
        expect(same_state(game.get_state(), before), "rejected roll_dice keeps the state");

        game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
        game.submit_layout(core::Side::Blue, {1, 2, 3, 4, 5, 6});
        game.roll_dice();
        expect(game.get_state().current_dice.has_value(), "roll_dice still works in ThrowingDice");
    }

    return test_support::finish();
}
