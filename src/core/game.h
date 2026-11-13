#ifndef GAME_H_
#define GAME_H_

#include <array>
#include <random>

#include "game_state.h"

namespace core {

class Game {
public:
    Game();

    void prepare_new_game(Side player1_side);

    void submit_layout(Side side, const std::array<int, 6>& layout);

    void load_game(GameState game_state);

    void roll_dice();

    void make_move(Pos from, Pos to);

    [[nodiscard]] const GameState& get_state() const;

private:
    void check_both_ready();

    GameState game_state_;
    std::mt19937 gen_;
};

}  // namespace core

#endif