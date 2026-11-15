#include "ai.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <vector>

#include "core/board.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"

namespace ai {

std::array<int, 6> decide_layout() {
    // 先构造顺序排列再打乱，结果天然是 1~6 的合法排列，无需额外校验。
    std::array<int, 6> layout{1, 2, 3, 4, 5, 6};
    // static：随机引擎只初始化一次。若每次调用都重建并重新播种，
    // 同一时间内的多次调用可能拿到相同种子，产生相同的"随机"序列。
    static std::mt19937 gen(std::random_device{}());
    std::shuffle(layout.begin(), layout.end(), gen);
    return layout;
}

Move decide_move(const core::GameState& state) {
    // 走子只在"某一方思考中"的阶段有语义；布局、掷骰子、对局结束阶段一律拒绝。
    if (state.phase != core::Phase::Player1Thinking &&
        state.phase != core::Phase::Player2Thinking) {
        throw std::invalid_argument("现在还不能移动");
    }

    // 暴力收集全部合法着法：遍历己方每个棋子的 8 邻域。
    // 合法性完全交给 core::rules::check_move 判定：边界、归属、
    // 骰子限定的可选性、方向。规则只在 core 里维护一份，
    // 避免 AI 侧出现会与正式规则漂移的第二套判断。
    std::vector<Move> legal_moves;
    core::Side ai_side = state.current_side;

    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            core::Pos from{x, y};
            if (state.board.at(from).side == ai_side) {
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy == 0) {
                            continue;  // 原地不动不是一步棋
                        }
                        // nx/ny 可能为 -1，转成 size_t 会得到极大值。这是有意的：
                        // 交给 check_move 的边界检查统一拦截，不会真正用于下标访问棋盘。
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
    // 正常对局中每个棋子至少有一个可走方向，走到这里说明状态异常
    // （例如棋盘上根本没有该方棋子），抛异常让上层显式处理而不是返回无效着法。
    if (legal_moves.empty()) {
        throw std::runtime_error("AI 无路可走！");
    }

    // 占位策略：在全部合法着法中均匀随机挑一步，不考虑吃子、位置或胜负。
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<std::size_t> dist(0, legal_moves.size() - 1);
    return legal_moves[dist(gen)];
}

}  // namespace ai