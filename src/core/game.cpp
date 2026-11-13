#include "game.h"

#include <optional>
#include <random>

#include "board.h"
#include "game_state.h"
#include "rules.h"
#include "types.h"

namespace core {

Game::Game() : gen_(std::random_device{}()) {
}

void Game::new_game(Side player1_side) {
    game_state_.board.setup_initial();
    game_state_.player1_side = player1_side;
    game_state_.current_side = Side::Red;
    game_state_.phase = Phase::ThrowingDice;
    game_state_.current_dice = std::nullopt;
    game_state_.winner = Side::None;
}

void Game::load_game(GameState game_state) {
    game_state_ = game_state;
}

void Game::roll_dice() {
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

}  // namespace core