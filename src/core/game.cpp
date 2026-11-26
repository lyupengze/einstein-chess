/// @file src/core/game.cpp
/// @brief 对局门面的实现：阶段校验与结算。

#include "game.h"

#include <array>
#include <optional>
#include <random>
#include <stdexcept>

#include "board.h"
#include "game_state.h"
#include "rules.h"
#include "types.h"
#include "validation.h"

namespace core {

Game::Game() : gen_(std::random_device{}()) {
}

void Game::prepare_new_game(Side player1_side) {
    // 发起方必须是实际可下棋的一方；允许 None 会让后续 submit_layout 把
    // 所有提交都记到 player2_ready，游戏永远停在 LayOut。
    if (player1_side != Side::Red && player1_side != Side::Blue) {
        throw std::invalid_argument("player1_side 必须是红方或蓝方");
    }
    // 校验通过后再重置，失败时保留旧局（强异常保证）。
    game_state_ = GameState{};
    game_state_.player1_side = player1_side;
    game_state_.phase = Phase::LayOut;
}

void Game::submit_layout(Side side, const std::array<int, 6>& layout) {
    // 只有布阵阶段可以提交；否则会在对局中途覆盖棋盘，甚至让 GameOver "复活"。
    if (game_state_.phase != Phase::LayOut) {
        throw std::invalid_argument("现在不能提交布阵");
    }
    if (game_state_.player1_side != Side::Red && game_state_.player1_side != Side::Blue) {
        throw std::invalid_argument("发起方尚未设置");
    }
    if (side != Side::Red && side != Side::Blue) {
        throw std::invalid_argument("布阵方必须是红方或蓝方");
    }

    // 同侧只允许提交一次：再次提交会覆盖已摆好的棋子，ready 标志也会失真。
    const bool already_ready =
        (side == game_state_.player1_side) ? game_state_.player1_ready : game_state_.player2_ready;
    if (already_ready) {
        throw std::invalid_argument("该方已经提交过布阵");
    }

    // setup_custom 内部会校验 side 与排列合法性，非法时抛异常且不修改状态。
    game_state_.board.setup_custom(side, layout);

    // 按"是否是我方"记录就绪标志，而不是按红蓝——AI 永远站在另一侧。
    if (side == game_state_.player1_side) {
        game_state_.player1_ready = true;
    } else {
        game_state_.player2_ready = true;
    }

    check_both_ready();
}

void Game::load_game(GameState game_state) {
    // 即使调用方来自 storage 也重新校验：非法局面在进入对局对象前被拒绝。
    require_valid_state(game_state);
    game_state_ = game_state;
}

void Game::roll_dice() {
    // 只有"等待掷骰"阶段可以掷骰；重复点击按钮或 UI 时序错乱都会走到这里。
    if (game_state_.phase != Phase::ThrowingDice) {
        throw std::invalid_argument("当前阶段不能掷骰");
    }
    // 阶段正确不代表当前方有效；None/未知阵营无法决定进入哪一方的思考阶段。
    if (game_state_.current_side != Side::Red && game_state_.current_side != Side::Blue) {
        throw std::invalid_argument("当前行棋方无效，不能掷骰子");
    }
    // std::uniform_int_distribution 的区间两端都包含，正好覆盖 1~6。
    game_state_.current_dice = std::uniform_int_distribution<int>(1, 6)(gen_);
    if (game_state_.current_side == game_state_.player1_side) {
        game_state_.phase = Phase::Player1Thinking;
    } else {
        game_state_.phase = Phase::Player2Thinking;
    }
}

GameState apply_move_and_settle(const GameState& state, Pos from, Pos to) {
    // 非法走法在这里抛出，state 不受影响（apply_move 先校验后修改）。
    GameState next = state;
    rules::apply_move(next, from, to);

    // 先判胜：胜负已分就不应再换手，否则 UI 会闪一下"轮到对方"。
    next.winner = rules::check_winner(next);
    if (next.winner == Side::None) {
        // 换手后回到掷骰阶段，由下一次 roll_dice 生成新的点数。
        if (next.current_side == Side::Red) {
            next.current_side = Side::Blue;
        } else {
            next.current_side = Side::Red;
        }
        next.phase = Phase::ThrowingDice;
    } else {
        next.phase = Phase::GameOver;
    }
    // 本回合骰子作废；读取 current_dice 前必须先确认 optional 有值。
    next.current_dice = std::nullopt;
    return next;
}

void Game::make_move(Pos from, Pos to) {
    // 走子只能发生在某一方的思考阶段。LayOut/ThrowingDice/GameOver 下若仍放行，
    // 会绕过"先掷骰再走子"的状态机，甚至终局后继续改棋盘。
    if (game_state_.phase != Phase::Player1Thinking &&
        game_state_.phase != Phase::Player2Thinking) {
        throw std::invalid_argument("现在不能走子");
    }
    if (!game_state_.current_dice.has_value()) {
        throw std::invalid_argument("当前回合没有骰子点数，不能走子");
    }
    if (game_state_.current_side != Side::Red && game_state_.current_side != Side::Blue) {
        throw std::invalid_argument("当前行棋方无效，不能走子");
    }
    // 结算顺序只在 apply_move_and_settle 中维护一份，AI 搜索复用同一函数。
    game_state_ = apply_move_and_settle(game_state_, from, to);
}

const GameState& Game::get_state() const {
    return game_state_;
}

void Game::check_both_ready() {
    // 只有布阵阶段才允许由"双方就绪"触发开局。否则 GameOver 之后再次提交
    // 会把阶段改回 ThrowingDice，让已经结束的对局复活。
    if (game_state_.phase != Phase::LayOut) {
        return;
    }
    if (game_state_.player1_ready && game_state_.player2_ready) {
        // 双方布阵完成后才开赛：进入掷骰阶段，先手固定为红方。
        game_state_.phase = Phase::ThrowingDice;
        game_state_.current_side = Side::Red;
    }
}

}  // namespace core