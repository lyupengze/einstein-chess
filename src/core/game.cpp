#include "game.h"

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
    game_state_ = GameState{};
    game_state_.player1_side = player1_side;
    game_state_.phase = Phase::LayOut;
}

void Game::submit_layout(Side side, const std::array<int, 6>& layout) {
    game_state_.board.setup_custom(side, layout);

    if (side == game_state_.player1_side) {
        game_state_.player1_ready = true;
    } else {
        game_state_.player2_ready = true;
    }

    check_both_ready();
}

void Game::load_game(GameState game_state) {
    game_state_ = game_state;
}

void Game::roll_dice() {
    if (game_state_.phase != Phase::ThrowingDice) {
        throw std::invalid_argument("现在不能扔骰子！");
    }
    game_state_.current_dice = std::uniform_int_distribution<int>(1, 6)(gen_);
    if (game_state_.current_side == game_state_.player1_side) {
        game_state_.phase = Phase::Player1Thinking;
    } else {
        game_state_.phase = Phase::Player2Thinking;
    }
}

void Game::make_move(Pos from, Pos to) {
    rules::apply_move(game_state_, from, to);
    game_state_.winner = rules::check_winner(game_state_);
    if (game_state_.winner == Side::None) {
        if (game_state_.current_side == Side::Red) {
            game_state_.current_side = Side::Blue;
        } else {
            game_state_.current_side = Side::Red;
        }
        game_state_.phase = Phase::ThrowingDice;
    } else {
        game_state_.phase = Phase::GameOver;
    }
    game_state_.current_dice = std::nullopt;
}

const GameState& Game::get_state() const {
    return game_state_;
}

void Game::check_both_ready() {
    if (game_state_.player1_ready && game_state_.player2_ready) {
        game_state_.phase = Phase::ThrowingDice;
        game_state_.current_side = Side::Red;
    }
}

}  // namespace core