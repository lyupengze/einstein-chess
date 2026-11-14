#ifndef RULES_H_
#define RULES_H_

#include <string>
#include <vector>

#include "game_state.h"

namespace core::rules {

[[nodiscard]] std::vector<int> get_selectable_ids(const GameState& state);

[[nodiscard]] std::string check_move(const GameState& state, Pos from, Pos to);

void apply_move(GameState& state, Pos from, Pos to);

[[nodiscard]] Side check_winner(const GameState& state);

}  // namespace core::rules

#endif