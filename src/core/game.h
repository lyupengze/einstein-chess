#ifndef GAME_H_
#define GAME_H_

#include <random>

#include "game_state.h"

namespace core {

class Game {
public:
    Game();

    void new_game(Side player1_side);

    void load_game(GameState game_state);

    void roll_dice();

    void make_move(Pos from, Pos to);

    [[nodiscard]] const GameState& get_state() const;

private:
    GameState game_state_;
    std::mt19937 gen_;
};

}  // namespace core

#endif