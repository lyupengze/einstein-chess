#include "ai.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"

namespace ai {

namespace {

// 评估权重集中在这里，后续调参只改这一处。
constexpr double kWinScore = 1.0e9;
constexpr double kPieceValue = 100.0;
constexpr double kProgressWeight = 8.0;
constexpr double kClosestWeight = 60.0;
constexpr double kTempoWeight = 10.0;
constexpr int kMaxDistance = static_cast<int>(core::Board::kSize) - 1;
constexpr double kTieEpsilon = 1.0e-9;

// 线程局部的随机引擎。原来的写法是函数内 static：只在首次初始化，这一点是对的，
// 但它是所有线程共享的同一份状态——并发决策会产生数据竞争（std::mt19937 不保证
// 线程安全），测试里也无法通过固定种子复现。thread_local 让每个线程各持一份，
// 既保留“只播种一次”，又消除了共享。
std::mt19937& engine() {
    thread_local std::mt19937 gen(std::random_device{}());
    return gen;
}

// 到对方目标角的最少步数。斜走一格同时改变 x/y，所以是 Chebyshev 距离。
int distance_to_goal(core::Pos pos, core::Side side) {
    if (side == core::Side::Red) {
        return std::max(kMaxDistance - static_cast<int>(pos.x),
                        kMaxDistance - static_cast<int>(pos.y));
    }
    if (side == core::Side::Blue) {
        return std::max(static_cast<int>(pos.x), static_cast<int>(pos.y));
    }
    return kMaxDistance;
}

double terminal_value(core::Side winner, core::Side root, int depth) {
    if (winner == core::Side::None) {
        return 0.0;
    }
    // 加/减 depth 让“更早获胜”优于“更晚获胜”，“更晚失败”优于“更早失败”。
    if (winner == root) {
        return kWinScore + static_cast<double>(depth);
    }
    return -kWinScore - static_cast<double>(depth);
}

double evaluate(const core::GameState& state, core::Side root) {
    if (state.winner != core::Side::None) {
        return terminal_value(state.winner, root, 0);
    }

    double score = 0.0;
    int my_closest = kMaxDistance + 1;
    int opp_closest = kMaxDistance + 1;

    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const core::Piece& piece = state.board.at({x, y});
            if (piece.side == core::Side::None) {
                continue;
            }
            const core::Pos pos{x, y};
            const int dist = distance_to_goal(pos, piece.side);
            const double value =
                kPieceValue + kProgressWeight * static_cast<double>(kMaxDistance - dist);
            if (piece.side == root) {
                score += value;
                my_closest = std::min(my_closest, dist);
            } else {
                score -= value;
                opp_closest = std::min(opp_closest, dist);
            }
        }
    }

    // 双方“最快棋子”的距离差是比赛跑的核心；只有一方能算出最近距离时跳过。
    if (my_closest <= kMaxDistance && opp_closest <= kMaxDistance) {
        score += kClosestWeight * static_cast<double>(opp_closest - my_closest);
    }
    // 当前行棋方有微弱先手优势。
    score += kTempoWeight * (state.current_side == root ? 1.0 : -1.0);
    return score;
}

double search_move_node(const core::GameState& state, int depth, core::Side root);
double search_chance_node(const core::GameState& state, int depth, core::Side root);

double search_move_node(const core::GameState& state, int depth, core::Side root) {
    if (depth <= 0) {
        return evaluate(state, root);
    }
    if (state.winner != core::Side::None) {
        return terminal_value(state.winner, root, depth);
    }

    const std::vector<core::Move> moves = core::rules::generate_legal_moves(state);
    if (moves.empty()) {
        // 正常对局不会出现；搜索到异常局面时退化为静态评估，而不是抛异常中断整次搜索。
        return evaluate(state, root);
    }

    const bool maximizing = (state.current_side == root);
    double best = maximizing ? -std::numeric_limits<double>::infinity()
                             : std::numeric_limits<double>::infinity();
    for (const core::Move& move : moves) {
        const core::GameState child = core::apply_move_and_settle(state, move.from, move.to);
        double value = 0.0;
        if (child.winner != core::Side::None) {
            value = terminal_value(child.winner, root, depth);
        } else {
            value = search_chance_node(child, depth - 1, root);
        }
        if (maximizing) {
            best = std::max(best, value);
        } else {
            best = std::min(best, value);
        }
    }
    return best;
}

double search_chance_node(const core::GameState& state, int depth, core::Side root) {
    if (depth <= 0) {
        return evaluate(state, root);
    }
    if (state.winner != core::Side::None) {
        return terminal_value(state.winner, root, depth);
    }

    // 骰子对双方公开，但当前方要先知道点数才能选子，因此 CHANCE 在走子节点之前。
    // 复用同一个副本逐点设置；search_move_node 只读，不会修改它。
    core::GameState with_dice = state;
    double sum = 0.0;
    for (int dice = 1; dice <= core::Board::kChessCount; ++dice) {
        with_dice.current_dice = dice;
        sum += search_move_node(with_dice, depth, root);
    }
    return sum / static_cast<double>(core::Board::kChessCount);
}

core::Move choose_move(const core::GameState& state, const SearchConfig& config) {
    const std::vector<core::Move> moves = core::rules::generate_legal_moves(state);
    if (moves.empty()) {
        throw std::runtime_error("AI 无路可走！");
    }
    if (moves.size() == 1) {
        return moves.front();
    }

    // 根节点行棋方必须取 current_side：Botzone 适配层会把 player1_side 固定为红方。
    const core::Side root = state.current_side;
    const int depth = std::max(1, config.max_depth);
    double best = -std::numeric_limits<double>::infinity();
    std::vector<core::Move> best_moves;

    for (const core::Move& move : moves) {
        const core::GameState child = core::apply_move_and_settle(state, move.from, move.to);
        double value = 0.0;
        if (child.winner != core::Side::None) {
            value = terminal_value(child.winner, root, depth);
        } else {
            value = search_chance_node(child, depth - 1, root);
        }

        if (best_moves.empty() || value > best + kTieEpsilon) {
            best = value;
            best_moves.clear();
            best_moves.push_back(move);
        } else if (value > best - kTieEpsilon) {
            best_moves.push_back(move);
        }
    }

    // 完全相同的评分下随机打破平局，避免每局走成同一条固定路线。
    std::uniform_int_distribution<std::size_t> dist(0, best_moves.size() - 1);
    return best_moves[dist(engine())];
}

}  // namespace

void set_random_seed(unsigned int seed) {
    engine() = std::mt19937(seed);
}

std::array<int, 6> decide_layout() {
    // 先构造顺序排列再打乱，结果天然是 1~6 的合法排列，无需额外校验。
    std::array<int, 6> layout{1, 2, 3, 4, 5, 6};
    std::shuffle(layout.begin(), layout.end(), engine());
    return layout;
}

core::Move decide_move(const core::GameState& state) {
    return decide_move(state, SearchConfig{});
}

core::Move decide_move(const core::GameState& state, const SearchConfig& config) {
    // 走子只在“某一方思考中”的阶段有语义；布局、掷骰子、对局结束阶段一律拒绝。
    if (state.phase != core::Phase::Player1Thinking &&
        state.phase != core::Phase::Player2Thinking) {
        throw std::invalid_argument("现在还不能移动");
    }
    return choose_move(state, config);
}

}  // namespace ai
