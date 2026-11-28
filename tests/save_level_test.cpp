/// @file tests/save_level_test.cpp
/// @brief 存档档位与随机流回归：随存档往返，缺失或损坏时按数据损坏拒绝。
///
/// 直接操作临时 SQLite 文件：先用 SaveManager 正常存档，再借 sqlite_wrapper 改写
/// data 列，观察 SaveManager 的解析行为。第二行末尾的三个字段（AI 档位、随机种子、
/// 已掷次数）都是必填字段，缺失、越界或非整数都按损坏数据拒绝。
/// 相关背景见 `#save-level-overflow` 与 `#seed-not-affecting-dice`。

#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ai/ai.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/types.h"
#include "storage/save_manager.h"
#include "storage/sqlite_wrapper.h"
#include "test_support.h"

using test_support::expect;

namespace {

/// @brief 测试专用临时数据库；放在测试的当前工作目录（构建目录）下。
constexpr const char* kDbPath = "test_save_level.db";

/// @brief 能通过语义校验的简单局面：轮到红方思考且骰子为 1。
core::GameState make_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 1;
    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({4, 4}) = {core::Side::Blue, 1};
    return state;
}

/// @brief 读出某槽位的原始序列化文本，绕过 SaveManager 以便伪造损坏数据。
std::string read_data(storage::Database& db, const std::string& slot) {
    storage::Statement stmt(db, "SELECT data FROM saves WHERE name = ?;");
    stmt.bind(1, slot);
    if (!stmt.step()) {
        throw std::runtime_error("测试数据缺失: " + slot);
    }
    return stmt.column_text(0);
}

/// @brief 覆盖式写入某槽位的原始序列化文本。
void write_data(storage::Database& db, const std::string& slot, const std::string& data) {
    storage::Statement stmt(db, "INSERT OR REPLACE INTO saves (name, data) VALUES (?, ?);");
    stmt.bind(1, slot);
    stmt.bind(2, data);
    stmt.step();
}

/// @brief 拆开的存档文本：第一行原样保留，第二行按空格拆成 token。
struct SplitData {
    std::string head;
    std::vector<std::string> tokens;
};

SplitData split_tail(const std::string& data) {
    const auto newline = data.find('\n');
    if (newline == std::string::npos) {
        throw std::runtime_error("测试数据格式异常");
    }
    SplitData split;
    split.head = data.substr(0, newline + 1);
    std::istringstream line(data.substr(newline + 1));
    for (std::string token; line >> token;) {
        split.tokens.push_back(token);
    }
    return split;
}

std::string join_tail(const SplitData& split) {
    std::string text = split.head;
    for (std::size_t i = 0; i < split.tokens.size(); ++i) {
        if (i != 0) {
            text += ' ';
        }
        text += split.tokens[i];
    }
    text += '\n';
    return text;
}

/// @brief 改写第二行第 index 个 token（0 起算）。
std::string replace_tail_token(const std::string& data, std::size_t index,
                               const std::string& replacement) {
    SplitData split = split_tail(data);
    if (index >= split.tokens.size()) {
        throw std::runtime_error("测试数据缺少指定的 token");
    }
    split.tokens[index] = replacement;
    return join_tail(split);
}

/// @brief 删除第二行第 index 个 token（0 起算）。
std::string erase_tail_token(const std::string& data, std::size_t index) {
    SplitData split = split_tail(data);
    if (index >= split.tokens.size()) {
        throw std::runtime_error("测试数据缺少指定的 token");
    }
    split.tokens.erase(split.tokens.begin() + static_cast<std::ptrdiff_t>(index));
    return join_tail(split);
}

/// @brief 第二行里三个尾部字段的下标：25 个棋盘格之后依次是档位、种子、已掷次数。
constexpr std::size_t kLevelIndex = core::Board::kSize * core::Board::kSize;
constexpr std::size_t kSeedIndex = kLevelIndex + 1;
constexpr std::size_t kRollsIndex = kLevelIndex + 2;

}  // namespace

int main() {
    // 上一轮异常退出可能留下同名文件，先清掉保证测试从空库开始。
    std::remove(kDbPath);

    const core::GameState state = make_state();
    constexpr core::RngState kRng{7, 3};

    {
        storage::SaveManager manager(kDbPath);
        manager.save_game("random", state, ai::Level::Random, kRng);
        manager.save_game("search", state, ai::Level::Search, kRng);

        // 1. 档位必须随存档往返，且两种档位各自独立。
        const auto random_saved = manager.load_game("random");
        expect(random_saved.has_value(), "随机档存档可读");
        expect(random_saved && random_saved->ai_level == ai::Level::Random, "随机档存档恢复随机档");

        const auto search_saved = manager.load_game("search");
        expect(search_saved && search_saved->ai_level == ai::Level::Search, "搜索档存档恢复搜索档");

        // 2. 随机流必须随存档往返：种子与已掷次数都原样恢复。
        expect(random_saved && random_saved->rng.seed == kRng.seed, "随机种子随存档往返");
        expect(random_saved && random_saved->rng.rolls == kRng.rolls, "已掷次数随存档往返");

        // 3. 局面本身不能被尾部字段影响。
        expect(random_saved && random_saved->state.phase == state.phase &&
                   random_saved->state.current_dice == state.current_dice,
               "局面随档位一起往返");

        // 下面用原始 SQL 伪造缺失字段与损坏值，因此先把 data 取出来。
        {
            storage::Database db(kDbPath);
            const std::string data = read_data(db, "random");

            write_data(db, "missing-level", erase_tail_token(data, kLevelIndex));
            write_data(db, "missing-seed", erase_tail_token(data, kSeedIndex));
            write_data(db, "missing-rolls", erase_tail_token(data, kRollsIndex));
            write_data(db, "bad-level", replace_tail_token(data, kLevelIndex, "9"));
            write_data(db, "garbage-level", replace_tail_token(data, kLevelIndex, "abc"));
            write_data(db, "overflow-level", replace_tail_token(data, kLevelIndex, "2147483648"));
            write_data(db, "negative-zero-seed", replace_tail_token(data, kSeedIndex, "-0"));
            write_data(db, "overflow-seed", replace_tail_token(data, kSeedIndex, "4294967296"));
            write_data(db, "overflow-rolls", replace_tail_token(data, kRollsIndex, "1000001"));
            write_data(db, "garbage-rolls", replace_tail_token(data, kRollsIndex, "1abc"));
        }

        const auto expect_rejected = [&manager](const std::string& slot, std::string_view what) {
            bool threw = false;
            try {
                [[maybe_unused]] const auto& ignored = manager.load_game(slot);
            } catch (const std::runtime_error&) {
                threw = true;
            }
            expect(threw, what);
        };

        expect_rejected("missing-level", "缺少档位的存档被拒绝");
        expect_rejected("missing-seed", "缺少随机种子的存档被拒绝");
        expect_rejected("missing-rolls", "缺少已掷次数的存档被拒绝");
        expect_rejected("bad-level", "越界档位被拒绝");
        expect_rejected("garbage-level", "非整数档位被拒绝");
        expect_rejected("overflow-level", "溢出档位被拒绝");
        expect_rejected("negative-zero-seed", "负数零种子被拒绝");
        expect_rejected("overflow-seed", "超出 32 位的种子被拒绝");
        expect_rejected("overflow-rolls", "超出上限的已掷次数被拒绝");
        expect_rejected("garbage-rolls", "非整数已掷次数被拒绝");
    }

    // 连接全部析构后再删文件，否则 Windows 上会因文件被占用而失败。
    std::remove(kDbPath);

    return test_support::finish();
}
