// 局面状态校验回归：验证 core::validate_state 能识别“字段都合法、组合起来
// 却不可能来自正常对局”的状态，并确认 Game::load_game 会在入口处拒绝它们。
//
// 这些用例对应审计里发现的真实后果：
// - Player2Thinking 却轮到 player1_side，GUI 会显示人类可走但点击被忽略；
// - current_side=None 的思考局面，CLI 会在 decide_move 抛“AI 无路可走！”；
// - GameOver 没有胜者，界面会显示“无 获胜！”。
// 正常夹具（含空棋盘 LayOut、部分布阵、终局）必须继续通过，避免校验器误伤。

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/types.h"
#include "core/validation.h"
#include "test_support.h"

using test_support::expect;

namespace {

/// @brief 一个正常的人类（红方）思考局面：棋盘上各有一枚棋子，骰子为 3。
core::GameState make_thinking_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 3;
    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({4, 4}) = {core::Side::Blue, 1};
    return state;
}

/// @brief 正常布阵阶段：允许空/部分棋盘、current_side=None、ready 不一致。
core::GameState make_layout_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::None;
    state.phase = core::Phase::LayOut;
    state.player1_ready = false;
    state.player2_ready = true;
    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({1, 1}) = {core::Side::Red, 2};
    state.board.at({4, 4}) = {core::Side::Blue, 1};
    return state;
}

/// @brief 正常终局：蓝方占 (0,0) 获胜，current_side=None、无骰子。
core::GameState make_game_over_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::None;
    state.phase = core::Phase::GameOver;
    state.winner = core::Side::Blue;
    state.player1_ready = true;
    state.player2_ready = true;
    state.board.at({0, 0}) = {core::Side::Blue, 1};
    state.board.at({0, 1}) = {core::Side::Red, 2};
    state.board.at({4, 4}) = {core::Side::Blue, 3};
    return state;
}

/// @brief 断言 state 至少产生一条 field 对应的问题。
void expect_issue(const core::GameState& state, std::string_view field, std::string_view what) {
    const std::vector<core::StateIssue> issues = core::validate_state(state);
    bool found = false;
    for (const core::StateIssue& issue : issues) {
        if (issue.field == field) {
            found = true;
        }
    }
    expect(found, std::string(what) + " (field=" + std::string(field) + ")");
}

/// @brief 断言 state 没有任何问题。
void expect_valid(const core::GameState& state, std::string_view what) {
    expect(core::validate_state(state).empty(), what);
}

/// @brief Game::load_game 是否以 invalid_argument 拒绝该状态。
bool load_throws(const core::GameState& state) {
    try {
        core::Game game;
        game.load_game(state);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    // 1. 正常状态必须全部通过，特别是空/部分棋盘的 LayOut 与无骰子的终局。
    expect_valid(make_thinking_state(), "normal thinking state is valid");
    expect_valid(make_layout_state(), "partial layout state is valid");
    expect_valid(make_game_over_state(), "normal game-over state is valid");

    {
        core::GameState empty_layout;
        empty_layout.player1_side = core::Side::Red;
        empty_layout.current_side = core::Side::None;
        empty_layout.phase = core::Phase::LayOut;
        expect_valid(empty_layout, "empty layout board is valid");
    }

    // 2. player1_side 不能是 None。
    {
        core::GameState state = make_thinking_state();
        state.player1_side = core::Side::None;
        expect_issue(state, "player1_side", "player1_side=None is rejected");
    }

    // 3. 掷骰阶段必须有可实际行棋的当前方。
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::ThrowingDice;
        state.current_side = core::Side::None;
        state.current_dice = std::nullopt;
        expect_issue(state, "current_side", "ThrowingDice without a current side is rejected");
    }

    // 4. Player1Thinking 必须轮到 player1_side。
    {
        core::GameState state = make_thinking_state();
        state.current_side = core::Side::Blue;
        expect_issue(state, "current_side", "Player1Thinking on the wrong side is rejected");
    }

    // 5. Player2Thinking 必须轮到另一方，不能又轮到 player1_side。
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::Player2Thinking;
        state.current_side = core::Side::Red;
        expect_issue(state, "current_side", "Player2Thinking on player1 side is rejected");
    }

    // 6. 思考阶段不能缺骰子；非思考阶段不能带骰子。
    {
        core::GameState state = make_thinking_state();
        state.current_dice = std::nullopt;
        expect_issue(state, "current_dice", "thinking without a dice is rejected");
    }
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::ThrowingDice;
        expect_issue(state, "current_dice", "ThrowingDice carrying a dice is rejected");
    }

    // 7. winner 与阶段必须一致。
    {
        core::GameState state = make_game_over_state();
        state.winner = core::Side::None;
        expect_issue(state, "winner", "GameOver without a winner is rejected");
    }
    {
        core::GameState state = make_thinking_state();
        state.winner = core::Side::Red;
        expect_issue(state, "winner", "non-GameOver state with a winner is rejected");
    }
    {
        core::GameState state = make_game_over_state();
        state.winner = core::Side::Red;  // 棋盘实际是蓝方占角
        expect_issue(state, "winner", "GameOver winner mismatching the board is rejected");
    }

    // 8. 非终局棋盘不能已经满足胜利条件。
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::ThrowingDice;
        state.current_side = core::Side::Red;
        state.current_dice = std::nullopt;
        state.board.at({4, 4}) = {core::Side::Red, 1};  // 红方已占角
        expect_issue(state, "winner", "non-GameOver winning board is rejected");
    }

    // 9. 棋盘数据本身：重复编号、越界编号、超过 6 子。
    {
        core::GameState state = make_thinking_state();
        state.board.at({0, 1}) = {core::Side::Red, 1};
        expect_issue(state, "board", "duplicate piece id is rejected");
    }
    {
        core::GameState state = make_thinking_state();
        state.board.at({0, 1}) = {core::Side::Red, 7};
        expect_issue(state, "board", "piece id outside 1..6 is rejected");
    }
    {
        core::GameState state = make_thinking_state();
        state.board = core::Board{};
        // 7 枚红子：1..6 各一枚，再重复一枚 1；坐标必须都在棋盘内。
        constexpr std::array<core::Pos, 7> positions{{
            {0, 0},
            {0, 1},
            {0, 2},
            {0, 3},
            {1, 0},
            {1, 1},
            {1, 2},
        }};
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const int id = (i < 6) ? static_cast<int>(i + 1) : 1;
            state.board.at(positions[i]) = {core::Side::Red, id};
        }
        expect_issue(state, "board", "more than six pieces on one side is rejected");
    }

    // 10. 非终局空棋盘与双方都空的终局都必须拒绝。
    {
        core::GameState state = make_thinking_state();
        state.board = core::Board{};
        expect_issue(state, "board", "empty non-LayOut board is rejected");
    }
    {
        core::GameState state;
        state.player1_side = core::Side::Red;
        state.current_side = core::Side::None;
        state.phase = core::Phase::GameOver;
        state.winner = core::Side::Blue;
        expect_issue(state, "board", "GameOver with both sides empty is rejected");
    }

    // 11. Game::load_game 是真正的入口：合法状态不抛，非法状态抛 invalid_argument。
    {
        bool loaded = true;
        try {
            core::Game game;
            game.load_game(make_thinking_state());
        } catch (const std::exception&) {
            loaded = false;
        }
        expect(loaded, "Game::load_game accepts a valid state");
    }
    {
        core::GameState state = make_thinking_state();
        state.player1_side = core::Side::None;
        expect(load_throws(state), "Game::load_game rejects an invalid state");
    }

    // 12. require_valid_state 的异常路径确实会被触发。
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::Player2Thinking;
        state.current_side = core::Side::Red;
        bool threw = false;
        try {
            core::require_valid_state(state);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        expect(threw, "require_valid_state throws for an invalid state");
    }

    return test_support::finish();
}
