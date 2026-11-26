/// @file tests/save_level_test.cpp
/// @brief 存档档位回归：档位随存档保存、读档原样恢复，非法档位被拒绝。
///
/// 直接操作临时 SQLite 文件：先用 SaveManager 正常存档，再借 sqlite_wrapper 把
/// data 列写入缺字段或损坏值，观察 SaveManager 的解析行为。档位是必填字段，
/// 缺失、越界或非整数都按损坏数据拒绝。相关背景见 `#save-level-overflow`。

#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "ai/ai.h"
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

/// @brief 定位最后一个字段（AI 档位）前的空格，跳过末尾换行。
std::size_t last_field_start(const std::string& data) {
    const auto newline = data.find_last_of('\n');
    const auto end = (newline == std::string::npos) ? data.size() : newline;
    const auto space = data.find_last_of(' ', end);
    if (space == std::string::npos) {
        throw std::runtime_error("测试数据格式异常");
    }
    return space;
}

/// @brief 去掉尾部的 AI 档位字段，模拟缺少必需字段的损坏存档。
std::string strip_ai_level(std::string data) {
    const auto space = last_field_start(data);
    const auto newline = data.find_last_of('\n');
    const auto end = (newline == std::string::npos) ? data.size() : newline;
    data.erase(space, end - space);
    return data;
}

/// @brief 把尾部 AI 档位写成任意文本，构造越界值与非法文本两种损坏。
std::string replace_ai_level(std::string data, const std::string& replacement) {
    const auto space = last_field_start(data);
    const auto newline = data.find_last_of('\n');
    const auto end = (newline == std::string::npos) ? data.size() : newline;
    data.replace(space, end - space, " " + replacement);
    return data;
}

}  // namespace

int main() {
    // 上一轮异常退出可能留下同名文件，先清掉保证测试从空库开始。
    std::remove(kDbPath);

    const core::GameState state = make_state();

    {
        storage::SaveManager manager(kDbPath);
        manager.save_game("random", state, ai::Level::Random);
        manager.save_game("search", state, ai::Level::Search);

        // 1. 档位必须随存档往返，且两种档位各自独立。
        const auto random_saved = manager.load_game("random");
        expect(random_saved.has_value(), "随机档存档可读");
        expect(random_saved && random_saved->ai_level == ai::Level::Random, "随机档存档恢复随机档");

        const auto search_saved = manager.load_game("search");
        expect(search_saved && search_saved->ai_level == ai::Level::Search, "搜索档存档恢复搜索档");

        // 2. 局面本身不能被档位字段影响。
        expect(random_saved && random_saved->state.phase == state.phase &&
                   random_saved->state.current_dice == state.current_dice,
               "局面随档位一起往返");

        // 下面用原始 SQL 伪造缺失字段与损坏档，因此先把 data 取出来。
        {
            storage::Database db(kDbPath);
            const std::string data = read_data(db, "random");

            // 3. 缺少尾部档位字段：必填字段缺失，不能静默回落。
            write_data(db, "missing-level", strip_ai_level(data));
            // 4. 档位越界：视为数据损坏，不能静默降级。
            write_data(db, "bad-level", replace_ai_level(data, "9"));
            // 5. 档位不是整数：同样视为数据损坏。
            write_data(db, "garbage", replace_ai_level(data, "abc"));
            // 6. 超出 int 的整数：operator>> 会置 failbit|eofbit，
            //    若按 EOF 处理会静默回落到默认档，因此必须拒绝。
            write_data(db, "overflow-level", replace_ai_level(data, "2147483648"));
        }

        bool missing_level_threw = false;
        try {
            [[maybe_unused]] const auto& ignored = manager.load_game("missing-level");
        } catch (const std::runtime_error&) {
            missing_level_threw = true;
        }
        expect(missing_level_threw, "缺少档位的存档被拒绝");

        bool bad_level_threw = false;
        try {
            [[maybe_unused]] const auto& ignored = manager.load_game("bad-level");
        } catch (const std::runtime_error&) {
            bad_level_threw = true;
        }
        expect(bad_level_threw, "越界档位被拒绝");

        bool garbage_threw = false;
        try {
            [[maybe_unused]] const auto& ignored = manager.load_game("garbage");
        } catch (const std::runtime_error&) {
            garbage_threw = true;
        }
        expect(garbage_threw, "非整数档位被拒绝");

        bool overflow_threw = false;
        try {
            [[maybe_unused]] const auto& ignored = manager.load_game("overflow-level");
        } catch (const std::runtime_error&) {
            overflow_threw = true;
        }
        expect(overflow_threw, "溢出档位被拒绝");
    }

    // 连接全部析构后再删文件，否则 Windows 上会因文件被占用而失败。
    std::remove(kDbPath);

    return test_support::finish();
}
