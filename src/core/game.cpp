#include "game.h"

#include <array>
#include <optional>
#include <random>
#include <stdexcept>

#include "board.h"
#include "game_state.h"
#include "rules.h"
#include "types.h"

namespace core {

Game::Game() : gen_(std::random_device{}()) {
}

void Game::prepare_new_game(Side player1_side) {
    // 整体重置为默认 GameState：空棋盘、双方未就绪、LayOut 阶段。
    game_state_ = GameState{};
    game_state_.player1_side = player1_side;
    game_state_.phase = Phase::LayOut;
}

void Game::submit_layout(Side side, const std::array<int, 6>& layout) {
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
    // 存档数据在 storage 反序列化时已做过完整性与范围校验，这里直接信任。
    game_state_ = game_state;
}

void Game::roll_dice() {
    // 只有"等待掷骰"阶段可以掷骰；重复点击按钮或 UI 时序错乱都会走到这里。
    if (game_state_.phase != Phase::ThrowingDice) {
        throw std::invalid_argument("现在不能扔骰子！");
    }
    // std::uniform_int_distribution 的区间两端都包含，正好覆盖 1~6。
    game_state_.current_dice = std::uniform_int_distribution<int>(1, 6)(gen_);
    if (game_state_.current_side == game_state_.player1_side) {
        game_state_.phase = Phase::Player1Thinking;
    } else {
        game_state_.phase = Phase::Player2Thinking;
    }
}

void Game::make_move(Pos from, Pos to) {
    // 非法走法在这里抛出，状态保持不变（apply_move 先校验后修改）。
    rules::apply_move(game_state_, from, to);

    // 先判胜：胜负已分就不应再换手，否则 UI 会闪一下"轮到对方"。
    game_state_.winner = rules::check_winner(game_state_);
    if (game_state_.winner == Side::None) {
        // 换手后回到掷骰阶段，由下一次 roll_dice 生成新的点数。
        if (game_state_.current_side == Side::Red) {
            game_state_.current_side = Side::Blue;
        } else {
            game_state_.current_side = Side::Red;
        }
        game_state_.phase = Phase::ThrowingDice;
    } else {
        game_state_.phase = Phase::GameOver;
    }
    // 本回合骰子作废；读取 current_dice 前必须先确认 optional 有值。
    game_state_.current_dice = std::nullopt;
}

const GameState& Game::get_state() const {
    return game_state_;
}

void Game::check_both_ready() {
    if (game_state_.player1_ready && game_state_.player2_ready) {
        // 双方布阵完成后才开赛：进入掷骰阶段，先手固定为红方。
        game_state_.phase = Phase::ThrowingDice;
        game_state_.current_side = Side::Red;
    }
}

}  // namespace core