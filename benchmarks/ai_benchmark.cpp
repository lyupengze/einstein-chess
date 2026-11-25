// AI 性能与强度基准。
//
// 目的不是“跑得好看”，而是给后续 AI 优化留下可复现的基线：
// - 单步延迟：开局、中局、残局各重复多次，输出中位数与 p95；
// - 对随机基线胜率：搜索版与随机版交替执红/执蓝自对弈，固定骰子序列与种子；
// - 所有局面先经过 core::require_valid_state，避免用非法状态测出假数据。
//
// 本目标只依赖 core/ai，不依赖 Qt，也不注册成 CTest 用例；正常构建会编译它，
// 但 CI 不运行计时，避免把机器噪声当成功能回归。

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "ai/ai.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"
#include "core/validation.h"

namespace {

/// 单步延迟测量的搜索深度，与默认 AI 保持一致。
constexpr int kSearchDepth = 4;
/// 每种局面重复次数；20 次足以看出中位数与长尾，又不至于让 benchmark 太慢。
constexpr int kLatencyRepeats = 20;
/// 自对弈总局数；红蓝各一半，降低先手偏差。
constexpr int kGameCount = 40;
/// 单局上限，防止异常局面无限循环；达到上限记为和棋。
constexpr int kMaxPlies = 160;
/// 固定随机种子，保证同一次构建下可复现。
constexpr unsigned int kRandomSeed = 0x5EED2026u;

/// 在棋盘上放置一枚棋子；benchmark 只构造已知合法局面，不做规则校验。
void put_piece(core::GameState& state, std::size_t x, std::size_t y, core::Side side, int id) {
    state.board.at({x, y}) = core::Piece{side, id};
}

/// 开局：6v6 初始布阵，红方掷出 5，轮到红方走子。
core::GameState make_opening_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 5;
    state.player1_ready = true;
    state.player2_ready = true;

    const std::array<int, core::Board::kChessCount> layout{1, 2, 3, 4, 5, 6};
    state.board.setup_custom(core::Side::Red, layout);
    state.board.setup_custom(core::Side::Blue, layout);
    return state;
}

/// 降级选子：去掉双方 5 号后掷出 5，4/6 两枚都可选，分支因子最大。
core::GameState make_fallback_state() {
    core::GameState state = make_opening_state();
    state.board.at({1, 1}) = core::Piece{core::Side::None, 0};
    state.board.at({3, 3}) = core::Piece{core::Side::None, 0};
    return state;
}

/// 中局：双方各 4 子，红方掷出 3，存在多个可选着法。
core::GameState make_midgame_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 3;
    state.player1_ready = true;
    state.player2_ready = true;

    put_piece(state, 0, 0, core::Side::Red, 1);
    put_piece(state, 1, 1, core::Side::Red, 2);
    put_piece(state, 2, 2, core::Side::Red, 3);
    put_piece(state, 3, 0, core::Side::Red, 4);
    put_piece(state, 4, 4, core::Side::Blue, 1);
    put_piece(state, 3, 3, core::Side::Blue, 2);
    put_piece(state, 2, 4, core::Side::Blue, 3);
    put_piece(state, 1, 4, core::Side::Blue, 4);
    return state;
}

/// 残局：双方各 2 子，红方掷出 2，搜索树明显更窄。
core::GameState make_endgame_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 2;
    state.player1_ready = true;
    state.player2_ready = true;

    put_piece(state, 2, 1, core::Side::Red, 1);
    put_piece(state, 3, 3, core::Side::Red, 2);
    put_piece(state, 2, 4, core::Side::Blue, 1);
    put_piece(state, 1, 2, core::Side::Blue, 2);
    return state;
}

/// 延迟统计结果；所有时间单位均为毫秒。
struct LatencyStats {
    double min_ms = 0.0;
    double median_ms = 0.0;
    double p95_ms = 0.0;
    double max_ms = 0.0;
};

/// 最近秩百分位；样本已升序排序。
double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) {
        return 0.0;
    }
    const auto rank =
        static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(sorted.size())));
    const std::size_t index = std::min(rank == 0 ? 0 : rank - 1, sorted.size() - 1);
    return sorted[index];
}

/// 测量一个局面的单步搜索延迟，并立即打印统计结果。
LatencyStats measure_latency(const core::GameState& state, std::string_view name) {
    ai::SearchConfig config;
    config.max_depth = kSearchDepth;

    // 先跑一次，排除首次分配/缓存冷启动对样本的干扰。
    const core::Move warmup = ai::decide_move(state, config);
    (void)warmup;

    std::vector<double> samples;
    samples.reserve(kLatencyRepeats);
    std::uint64_t checksum = 0;
    for (int i = 0; i < kLatencyRepeats; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const core::Move move = ai::decide_move(state, config);
        const auto end = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::milli> elapsed = end - start;
        samples.push_back(elapsed.count());
        checksum += static_cast<std::uint64_t>(move.from.x + move.from.y + move.to.x + move.to.y);
    }
    (void)checksum;

    std::sort(samples.begin(), samples.end());
    LatencyStats stats;
    stats.min_ms = samples.front();
    stats.median_ms = percentile(samples, 0.50);
    stats.p95_ms = percentile(samples, 0.95);
    stats.max_ms = samples.back();

    std::cout << std::left << std::setw(10) << name << std::right << std::fixed
              << std::setprecision(2) << " min=" << std::setw(7) << stats.min_ms
              << " median=" << std::setw(7) << stats.median_ms << " p95=" << std::setw(7)
              << stats.p95_ms << " max=" << std::setw(7) << stats.max_ms << " ms\n";
    return stats;
}

/// 自对弈统计；draws 包含达到步数上限、无合法着法和异常中断。
struct SelfPlayStats {
    int search_wins = 0;
    int random_wins = 0;
    int draws = 0;
    int errors = 0;
    int total_plies = 0;
    double elapsed_ms = 0.0;
};

/// 搜索版与随机版自对弈；红蓝各一半，骰子序列固定。
SelfPlayStats run_self_play() {
    SelfPlayStats stats;
    ai::set_random_seed(kRandomSeed);

    const auto start = std::chrono::steady_clock::now();
    for (int game = 0; game < kGameCount; ++game) {
        std::mt19937 dice_rng(0xD1CE0000u + static_cast<unsigned int>(game));
        std::uniform_int_distribution<int> dice_dist(1, 6);

        core::GameState state = make_opening_state();
        state.current_dice = dice_dist(dice_rng);

        const core::Side search_side = (game % 2 == 0) ? core::Side::Red : core::Side::Blue;
        bool aborted = false;
        int plies = 0;
        for (; plies < kMaxPlies; ++plies) {
            if (state.phase == core::Phase::GameOver) {
                break;
            }
            if (state.phase == core::Phase::ThrowingDice) {
                state.current_dice = dice_dist(dice_rng);
                state.phase = (state.current_side == state.player1_side)
                                  ? core::Phase::Player1Thinking
                                  : core::Phase::Player2Thinking;
            }

            const std::vector<core::Move> moves = core::rules::generate_legal_moves(state);
            if (moves.empty()) {
                aborted = true;
                break;
            }

            try {
                const ai::Level level =
                    (state.current_side == search_side) ? ai::Level::kSearch : ai::Level::kRandom;
                const core::Move move = ai::decide_move(state, level);
                state = core::apply_move_and_settle(state, move.from, move.to);
            } catch (const std::exception&) {
                ++stats.errors;
                aborted = true;
                break;
            }
        }
        stats.total_plies += plies;

        if (!aborted && state.phase == core::Phase::GameOver && state.winner == search_side) {
            ++stats.search_wins;
        } else if (!aborted && state.phase == core::Phase::GameOver &&
                   state.winner != core::Side::None) {
            ++stats.random_wins;
        } else {
            ++stats.draws;
        }
    }
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = end - start;
    stats.elapsed_ms = elapsed.count();
    return stats;
}

}  // namespace

int main() {
    try {
        const core::GameState opening = make_opening_state();
        const core::GameState fallback = make_fallback_state();
        const core::GameState midgame = make_midgame_state();
        const core::GameState endgame = make_endgame_state();
        core::require_valid_state(opening);
        core::require_valid_state(fallback);
        core::require_valid_state(midgame);
        core::require_valid_state(endgame);

        std::cout << "AI benchmark: depth=" << kSearchDepth << ", repeats=" << kLatencyRepeats
                  << "\n";
        measure_latency(opening, "opening");
        measure_latency(fallback, "fallback");
        measure_latency(midgame, "midgame");
        measure_latency(endgame, "endgame");

        std::cout << "\nSelf-play: search vs random, games=" << kGameCount
                  << ", max_plies=" << kMaxPlies << "\n";
        const SelfPlayStats stats = run_self_play();
        const double win_rate =
            100.0 * static_cast<double>(stats.search_wins) / static_cast<double>(kGameCount);
        const double average_plies =
            static_cast<double>(stats.total_plies) / static_cast<double>(kGameCount);
        std::cout << std::fixed << std::setprecision(2) << "search_wins=" << stats.search_wins
                  << " random_wins=" << stats.random_wins << " draws=" << stats.draws
                  << " errors=" << stats.errors << " win_rate=" << win_rate
                  << "% avg_plies=" << average_plies << " total_ms=" << stats.elapsed_ms << "\n";
        return stats.errors == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "benchmark failed: " << error.what() << "\n";
        return 1;
    }
}
