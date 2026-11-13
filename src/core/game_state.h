#ifndef GAME_STATE_H_
#define GAME_STATE_H_

#include <optional>

#include "board.h"
#include "types.h"

namespace core {

enum class Phase { LayOut, ThrowingDice, Player1Thinking, Player2Thinking, GameOver };

struct GameState {
    Board board;
    Side player1_side{Side::None};
    Side current_side{Side::None};
    Phase phase{Phase::LayOut};
    std::optional<int> current_dice;
    Side winner{Side::None};
    bool player1_ready{false};
    bool player2_ready{false};
};

}  // namespace core

#endif