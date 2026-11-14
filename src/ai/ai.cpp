#include "ai.h"

#include <algorithm>
#include <array>
#include <random>
#include <vector>

#include "core/board.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"

namespace ai {

std::array<int, 6> decide_layout() {
    std::array<int, 6> layout{1, 2, 3, 4, 5, 6};
    static std::mt19937 gen(std::random_device{}());
    std::shuffle(layout.begin(), layout.end(), gen);
    return layout;
}

Move decide_move(const core::GameState& state) {
    if (state.phase != core::Phase::Player1Thinking &&
        state.phase != core::Phase::Player2Thinking) {
        throw std::invalid_argument("现在还不能移动");
    }
    std::vector<Move> legal_moves;
    core::Side ai_side = state.current_side;

    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            core::Pos from{x, y};
            if (state.board.at(from).side == ai_side) {
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy == 0) {
                            continue;
                        }
                        int nx = static_cast<int>(x) + dx;
                        int ny = static_cast<int>(y) + dy;
                        core::Pos to{static_cast<std::size_t>(nx), static_cast<std::size_t>(ny)};
                        if (core::rules::check_move(state, from, to).empty()) {
                            legal_moves.push_back({from, to});
                        }
                    }
                }
            }
        }
    }
    if (legal_moves.empty()) {
        throw std::runtime_error("AI 无路可走！");
    }

    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<std::size_t> dist(0, legal_moves.size() - 1);
    return legal_moves[dist(gen)];
}

}  // namespace ai