/// @file tests/state_validation_test.cpp
/// @brief 局面校验回归：字段各自合法、组合起来却不可能来自正常对局的局面必须被拒绝。
///
/// 关注点是“入口是否拒绝”，不是具体文案：validate_state 负责收集问题，
/// Game::load_game 与 require_valid_state 负责把它们变成异常。正常夹具
/// （空棋盘布阵、部分布阵、终局）必须继续通过，避免校验器误伤。

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
    expect_valid(make_thinking_state(), "正常的思考局面通过校验");
    expect_valid(make_layout_state(), "部分布阵的布阵局面通过校验");
    expect_valid(make_game_over_state(), "正常终局通过校验");

    {
        core::GameState empty_layout;
        empty_layout.player1_side = core::Side::Red;
        empty_layout.current_side = core::Side::None;
        empty_layout.phase = core::Phase::LayOut;
        expect_valid(empty_layout, "空棋盘的布阵局面通过校验");
    }

    // 2. player1_side 不能是 None。
    {
        core::GameState state = make_thinking_state();
        state.player1_side = core::Side::None;
        expect_issue(state, "player1_side", "player1_side 为 None 被拒绝");
    }

    // 3. 掷骰阶段必须有可实际行棋的当前方。
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::ThrowingDice;
        state.current_side = core::Side::None;
        state.current_dice = std::nullopt;
        expect_issue(state, "current_side", "掷骰阶段缺少当前方被拒绝");
    }

    // 4. Player1Thinking 必须轮到 player1_side。
    {
        core::GameState state = make_thinking_state();
        state.current_side = core::Side::Blue;
        expect_issue(state, "current_side", "Player1Thinking 落到对方被拒绝");
    }

    // 5. Player2Thinking 必须轮到另一方，不能又轮到 player1_side。
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::Player2Thinking;
        state.current_side = core::Side::Red;
        expect_issue(state, "current_side", "Player2Thinking 落到我方被拒绝");
    }

    // 6. 思考阶段不能缺骰子；非思考阶段不能带骰子。
    {
        core::GameState state = make_thinking_state();
        state.current_dice = std::nullopt;
        expect_issue(state, "current_dice", "思考阶段缺少骰子被拒绝");
    }
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::ThrowingDice;
        expect_issue(state, "current_dice", "掷骰阶段带骰子被拒绝");
    }

    // 7. winner 与阶段必须一致。
    {
        core::GameState state = make_game_over_state();
        state.winner = core::Side::None;
        expect_issue(state, "winner", "终局没有胜者被拒绝");
    }
    {
        core::GameState state = make_thinking_state();
        state.winner = core::Side::Red;
        expect_issue(state, "winner", "非终局带胜者被拒绝");
    }
    {
        core::GameState state = make_game_over_state();
        state.winner = core::Side::Red;  // 棋盘实际是蓝方占角
        expect_issue(state, "winner", "终局胜者与棋盘不符被拒绝");
    }

    // 8. 非终局棋盘不能已经满足胜利条件。
    {
        core::GameState state = make_thinking_state();
        state.phase = core::Phase::ThrowingDice;
        state.current_side = core::Side::Red;
        state.current_dice = std::nullopt;
        state.board.at({4, 4}) = {core::Side::Red, 1};  // 红方已占角
        expect_issue(state, "winner", "非终局棋盘已满足胜利条件被拒绝");
    }

    // 9. 棋盘数据本身：重复编号、越界编号、超过 6 子。
    {
        core::GameState state = make_thinking_state();
        state.board.at({0, 1}) = {core::Side::Red, 1};
        expect_issue(state, "board", "同方重复编号被拒绝");
    }
    {
        core::GameState state = make_thinking_state();
        state.board.at({0, 1}) = {core::Side::Red, 7};
        expect_issue(state, "board", "编号超出 1~6 被拒绝");
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
        expect_issue(state, "board", "单方超过 6 枚棋子被拒绝");
    }

    // 10. 非终局空棋盘与双方都空的终局都必须拒绝。
    {
        core::GameState state = make_thinking_state();
        state.board = core::Board{};
        expect_issue(state, "board", "非布阵阶段的空棋盘被拒绝");
    }
    {
        core::GameState state;
        state.player1_side = core::Side::Red;
        state.current_side = core::Side::None;
        state.phase = core::Phase::GameOver;
        state.winner = core::Side::Blue;
        expect_issue(state, "board", "双方皆空的终局被拒绝");
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
        expect(loaded, "load_game 接受合法局面");
    }
    {
        core::GameState state = make_thinking_state();
        state.player1_side = core::Side::None;
        expect(load_throws(state), "load_game 拒绝非法局面");
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
        expect(threw, "require_valid_state 对非法局面抛异常");
    }

    // 13. 布阵阶段不能有当前行棋方，也不能双方都已就绪：双方都就绪却仍停在
    // 布阵，界面没有任何路径再触发开赛。
    {
        core::GameState state = make_layout_state();
        state.current_side = core::Side::Red;
        expect_issue(state, "current_side", "布阵阶段带当前方被拒绝");
    }
    {
        core::GameState state = make_layout_state();
        state.player1_ready = true;  // make_layout_state 的 player2_ready 已是 true
        expect_issue(state, "ready", "布阵阶段双方都已就绪被拒绝");
        expect(load_throws(state), "load_game 拒绝双方都已就绪的布阵局面");
    }

    return test_support::finish();
}
