#ifndef GAME_STATE_H_
#define GAME_STATE_H_

#include <optional>

#include "board.h"
#include "types.h"

namespace core {

enum class Phase { ThrowingDice, Player1Thinking, Player2Thinking, Moving };

struct GameState {
    Board board;
    Side player1_side{Side::None};
    Side current_side{Side::None};
    Phase phase{Phase::ThrowingDice};
    std::optional<int> current_dice;
    Side winner{Side::None};
};

}  // namespace core

#endif