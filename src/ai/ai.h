#ifndef AI_H_
#define AI_H_

#include <array>

#include "core/game_state.h"
#include "core/types.h"

namespace ai {

struct Move {
    core::Pos from;
    core::Pos to;
};

[[nodiscard]] std::array<int, 6> decide_layout();

[[nodiscard]] Move decide_move(const core::GameState& state);

}  // namespace ai

#endif