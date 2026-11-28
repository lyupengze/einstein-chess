/// @file tests/game_seed_test.cpp
/// @brief 锁定对局侧的种子语义：同种子骰序一致、异种子不同、已掷次数递增。
///
/// 骰子是对局侧唯一的随机源；AI 的随机源由 ai::set_random_seed 单独播种。
/// 这里只测对局对象自身，不经过存档或界面，避免确定性被别处的转换掩盖。

#include <array>
#include <cstdint>
#include <vector>

#include "core/game.h"
#include "core/rules.h"
#include "test_support.h"

namespace {

/// @brief 布好阵，返回处于掷骰阶段的对局。
/// @param seed 骰子随机源的种子
[[nodiscard]] core::Game make_ready_game(std::uint32_t seed) {
    core::Game game(seed);
    game.prepare_new_game(core::Side::Red);
    const std::array<int, core::Board::kChessCount> layout{1, 2, 3, 4, 5, 6};
    game.submit_layout(core::Side::Red, layout);
    game.submit_layout(core::Side::Blue, layout);
    return game;
}

/// @brief 连续掷 rolls 次，每掷一次走一步棋把阶段送回掷骰阶段。
/// @return 点数序列；对局提前结束时长度会小于 rolls
[[nodiscard]] std::vector<int> roll_sequence(std::uint32_t seed, int rolls) {
    core::Game game = make_ready_game(seed);
    std::vector<int> dice;
    for (int i = 0; i < rolls; ++i) {
        if (game.get_state().phase != core::Phase::ThrowingDice) {
            break;
        }
        game.roll_dice();
        dice.push_back(*game.get_state().current_dice);
        const std::vector<core::Move> moves = core::rules::generate_legal_moves(game.get_state());
        if (moves.empty()) {
            break;
        }
        game.make_move(moves.front().from, moves.front().to);
    }
    return dice;
}

}  // namespace

int main() {
    using test_support::expect;

    const std::vector<int> first = roll_sequence(42, 6);
    const std::vector<int> again = roll_sequence(42, 6);
    expect(first.size() == 6, "六次掷骰都完成");
    expect(first == again, "同一种子的骰序一致");

    bool any_diff = false;
    for (std::uint32_t seed = 43; seed < 48; ++seed) {
        if (roll_sequence(seed, 6) != first) {
            any_diff = true;
            break;
        }
    }
    expect(any_diff, "不同种子得到不同骰序");

    core::Game fresh(42);
    expect(fresh.seed() == 42, "seed() 返回构造时传入的种子");
    expect(fresh.roll_count() == 0, "新局尚未掷骰");

    core::Game counted = make_ready_game(7);
    counted.roll_dice();
    expect(counted.roll_count() == 1, "掷一次后计数为 1");
    const std::vector<core::Move> moves = core::rules::generate_legal_moves(counted.get_state());
    if (!moves.empty()) {
        counted.make_move(moves.front().from, moves.front().to);
        counted.roll_dice();
        expect(counted.roll_count() == 2, "再掷一次后计数为 2");
    }

    // 读档等价：先掷三次记下随机流，再在另一局里还原，下一掷必须与原来相同。
    core::Game original = make_ready_game(2026);
    for (int i = 0; i < 3; ++i) {
        original.roll_dice();
        const std::vector<core::Move> played =
            core::rules::generate_legal_moves(original.get_state());
        if (played.empty()) {
            break;
        }
        original.make_move(played.front().from, played.front().to);
    }
    const core::RngState snapshot = original.rng_state();
    const std::uint64_t rolls_before = original.roll_count();
    original.roll_dice();
    const int expected = *original.get_state().current_dice;

    core::Game restored = make_ready_game(999);
    restored.restore_rng_state(snapshot);
    expect(restored.roll_count() == rolls_before, "还原后已掷次数一致");
    restored.roll_dice();
    expect(restored.get_state().current_dice && *restored.get_state().current_dice == expected,
           "还原随机流后下一掷与原来一致");

    return test_support::finish();
}
