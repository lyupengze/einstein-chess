/// @file src/ai/ai.cpp
/// @brief 决策实现：竞速分布表、期望胜率评估与两个档位的走子策略。
///
/// 竞速表在首次使用时构建一次（46656 个单方状态），之后只读，评估路径因此没有
/// 额外分配；搜索是纯函数，不改传入局面，CHANCE 节点只改自己的副本。

#include "ai.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"

namespace ai {

namespace {

// 竞速模型：单方 6 个编号，0=被吃，1..5=到目标角的距离+1。
// 46656 = 6^6 个状态；表里存"按最小化期望到达时间走，第 t 回合获胜"的概率。
constexpr int kPieceCount = core::Board::kChessCount;
constexpr int kRaceMaxStep = 19;
constexpr int kRaceStateCount = 46656;
constexpr std::array<int, 6> kPower6{1, 6, 36, 216, 1296, 7776};
constexpr int kMaxDistance = static_cast<int>(core::Board::kSize) - 1;

// 终局值略高于/低于任何 [0,1] 的胜率，并让"更早获胜/更晚失败"更优。
constexpr double kTerminalEpsilon = 1.0e-6;
constexpr double kTieEpsilon = 1.0e-9;

using RaceDistribution = std::array<float, kRaceMaxStep + 1>;
using RaceTable = std::vector<RaceDistribution>;

// 随机源用线程局部而不是函数内 static：static 会被所有线程共享，
// 而 std::mt19937 不保证线程安全，测试也无法用固定种子复现；
// thread_local 保留“只播种一次”，又消除了共享。
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
    // 加/减 depth 让"更早获胜"优于"更晚获胜"，"更晚失败"优于"更早失败"。
    if (winner == root) {
        return 1.0 + kTerminalEpsilon * static_cast<double>(depth);
    }
    return -kTerminalEpsilon * static_cast<double>(depth);
}

// 把局面按"某一方"编码成竞速表下标；只使用几何距离，不看占位。
int race_state(const core::GameState& state, core::Side side) {
    std::array<int, kPieceCount> digits{};
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const core::Piece& piece = state.board.at({x, y});
            if (piece.side != side || piece.id < 1 || piece.id > kPieceCount) {
                continue;
            }
            digits[static_cast<std::size_t>(piece.id - 1)] = distance_to_goal({x, y}, side) + 1;
        }
    }

    int code = 0;
    for (int i = 0; i < kPieceCount; ++i) {
        code += digits[static_cast<std::size_t>(i)] * kPower6[static_cast<std::size_t>(i)];
    }
    return code;
}

// 打表：每个状态对 6 个点数各选期望最短的棋子，分布后移一回合再平均。
RaceTable build_race_table() {
    RaceTable table(kRaceStateCount);
    std::vector<double> expected_time(kRaceStateCount, 0.0);
    std::array<int, kPieceCount> digits{};
    std::array<double, kRaceMaxStep + 1> tmp{};

    for (int code = 1; code < kRaceStateCount; ++code) {
        for (int i = 0; i < kPieceCount; ++i) {
            digits[static_cast<std::size_t>(i)] = (code / kPower6[static_cast<std::size_t>(i)]) % 6;
        }

        // 任意一枚棋子已经在目标角，这一方已经赢了。
        if (std::find(digits.begin(), digits.end(), 1) != digits.end()) {
            table[static_cast<std::size_t>(code)][0] = 1.0F;
            continue;
        }

        tmp.fill(0.0);
        double total_expected = 0.0;
        for (int dice = 1; dice <= kPieceCount; ++dice) {
            const auto ids = core::rules::selectable_ids_for(
                dice, [&](int id) { return digits[static_cast<std::size_t>(id - 1)] != 0; });

            double best_expected = std::numeric_limits<double>::infinity();
            int best_next = -1;
            for (int id : ids) {
                if (id == -1) {
                    continue;
                }
                const int next = code - kPower6[static_cast<std::size_t>(id - 1)];
                if (expected_time[static_cast<std::size_t>(next)] < best_expected) {
                    best_expected = expected_time[static_cast<std::size_t>(next)];
                    best_next = next;
                }
            }

            // 非零状态至少有一枚棋子，降级选子必然能选到；这里只做防御。
            if (best_next == -1) {
                continue;
            }
            for (int step = 1; step <= kRaceMaxStep; ++step) {
                tmp[static_cast<std::size_t>(step)] +=
                    table[static_cast<std::size_t>(best_next)][static_cast<std::size_t>(step - 1)];
            }
            total_expected += best_expected + 1.0;
        }

        for (int step = 1; step <= kRaceMaxStep; ++step) {
            table[static_cast<std::size_t>(code)][static_cast<std::size_t>(step)] =
                static_cast<float>(tmp[static_cast<std::size_t>(step)] /
                                   static_cast<double>(kPieceCount));
        }
        expected_time[static_cast<std::size_t>(code)] =
            total_expected / static_cast<double>(kPieceCount);
    }
    return table;
}

const RaceTable& race_table() {
    static const RaceTable table = build_race_table();
    return table;
}

// 双方都按竞速表独立推进时的胜率：P(我方 T <= 对方 T)。
// 当前行棋方先手，所以 t 相同时算我方赢。
double win_rate(const core::GameState& state, core::Side root) {
    if (state.winner != core::Side::None) {
        return state.winner == root ? 1.0 : 0.0;
    }

    const core::Side mover = state.current_side;
    if (mover == core::Side::None) {
        return 0.0;
    }
    const core::Side opponent = (mover == core::Side::Red) ? core::Side::Blue : core::Side::Red;

    const int mover_state = race_state(state, mover);
    const int opponent_state = race_state(state, opponent);
    const RaceTable& table = race_table();

    double cumulative = 0.0;
    double mover_win = 0.0;
    for (int step = 0; step <= kRaceMaxStep; ++step) {
        cumulative += table[static_cast<std::size_t>(mover_state)][static_cast<std::size_t>(step)];
        mover_win +=
            cumulative *
            table[static_cast<std::size_t>(opponent_state)][static_cast<std::size_t>(step)];
    }
    return mover == root ? mover_win : 1.0 - mover_win;
}

double evaluate(const core::GameState& state, core::Side root) {
    return win_rate(state, root);
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
        throw std::invalid_argument("局面没有合法着法可走");
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

    // 竞速评估只返回胜率，必胜/必败时大量着法同分。同分先选走完后离终点更近的，
    // 再优先斜向，避免随机走进原地打转的轴向着法。
    int best_distance = std::numeric_limits<int>::max();
    for (const core::Move& move : best_moves) {
        best_distance = std::min(best_distance, distance_to_goal(move.to, root));
    }
    std::vector<core::Move> closest;
    for (const core::Move& move : best_moves) {
        if (distance_to_goal(move.to, root) == best_distance) {
            closest.push_back(move);
        }
    }
    std::vector<core::Move> diagonal;
    for (const core::Move& move : closest) {
        if (move.from.x != move.to.x && move.from.y != move.to.y) {
            diagonal.push_back(move);
        }
    }
    const std::vector<core::Move>& candidates = diagonal.empty() ? closest : diagonal;

    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
    return candidates[dist(engine())];
}

// 所有档位共用的前置检查：走子只在“某一方思考中”的阶段有语义，放在分发之前，
// 保证切换档位不改变调用方看到的错误行为。
void require_thinking_phase(const core::GameState& state) {
    if (state.phase != core::Phase::Player1Thinking &&
        state.phase != core::Phase::Player2Thinking) {
        throw std::invalid_argument("现在还不能移动");
    }
}

// 随机档的走子策略：在全部合法着法中均匀随机挑一步，不考虑吃子、位置或胜负。
// 合法着法枚举交给 core::rules，AI 不复制规则，换实现也不会产生非法着法。
core::Move choose_random_move(const core::GameState& state) {
    const std::vector<core::Move> moves = core::rules::generate_legal_moves(state);
    if (moves.empty()) {
        throw std::invalid_argument("局面没有合法着法可走");
    }
    std::uniform_int_distribution<std::size_t> dist(0, moves.size() - 1);
    return moves[dist(engine())];
}

}  // namespace

std::string level_name(Level level) {
    switch (level) {
        case Level::kRandom:
            return "随机";
        case Level::kSearch:
            return "搜索";
        default:
            // 兜住通过 static_cast 传入的越界值，避免界面显示空串。
            return "未知";
    }
}

double estimate_win_rate(const core::GameState& state) {
    // 对外接口固定按"当前行棋方"解释；search 内部则显式传根方。
    if (state.current_side == core::Side::None) {
        return 0.0;
    }
    return win_rate(state, state.current_side);
}

void set_random_seed(unsigned int seed) {
    engine() = std::mt19937(seed);
}

std::array<int, 6> decide_layout() {
    // 先构造顺序排列再打乱，结果天然是 1~6 的合法排列，无需额外校验。
    std::array<int, 6> layout{1, 2, 3, 4, 5, 6};
    std::shuffle(layout.begin(), layout.end(), engine());
    return layout;
}

core::Move decide_move(const core::GameState& state, Level level) {
    // 先统一校验阶段：两个档位对“现在能不能走”的判定必须完全一致。
    require_thinking_phase(state);

    switch (level) {
        case Level::kRandom:
            return choose_random_move(state);
        case Level::kSearch:
            return choose_move(state, SearchConfig{});
        default:
            // 枚举值由调用方 static_cast 伪造时显式报错，而不是静默降级到某个档位。
            throw std::invalid_argument("未知的 AI 档位");
    }
}

core::Move decide_move(const core::GameState& state, const SearchConfig& config) {
    require_thinking_phase(state);
    return choose_move(state, config);
}

}  // namespace ai
