// 存档强度回归：验证 AI 档位随存档一起保存、读档时原样恢复，
// 并且没有档位字段的旧存档仍能读出，按默认档位（当前最新版）继续。
//
// 这里直接操作临时 SQLite 文件：先用 SaveManager 正常存档，再借 sqlite_wrapper
// 把 data 列改成“旧格式”或“损坏值”，观察 SaveManager 的解析行为。
// 覆盖的三条路径正对应玩家会遇到的三种档案：新档、旧档、被改坏的档。

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

namespace {

/// 测试专用临时数据库；放在测试的当前工作目录（构建目录）下。
constexpr const char* kDbPath = "test_save_level.db";

int failures = 0;

void expect(bool cond, std::string_view what) {
    std::cout << (cond ? "PASS" : "FAIL") << ' ' << what << '\n';
    if (!cond) {
        ++failures;
    }
}

// 一个能通过反序列化语义校验的简单局面：轮到红方思考且骰子为 1。
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

/// 读出某槽位的原始序列化文本（绕过 SaveManager，便于伪造损坏数据）。
std::string read_data(storage::Database& db, const std::string& slot) {
    storage::Statement stmt(db, "SELECT data FROM saves WHERE name = ?;");
    stmt.bind(1, slot);
    if (!stmt.step()) {
        throw std::runtime_error("测试数据缺失: " + slot);
    }
    return stmt.column_text(0);
}

/// 覆盖式写入某槽位的原始序列化文本。
void write_data(storage::Database& db, const std::string& slot, const std::string& data) {
    storage::Statement stmt(db, "INSERT OR REPLACE INTO saves (name, data) VALUES (?, ?);");
    stmt.bind(1, slot);
    stmt.bind(2, data);
    stmt.step();
}

/// 序列化文本的最后一个字段是 AI 档位，定位它前面那个空格的位置。
/// 末尾可能有换行，判断边界时先跳过。
std::size_t last_field_start(const std::string& data) {
    const auto newline = data.find_last_of('\n');
    const auto end = (newline == std::string::npos) ? data.size() : newline;
    const auto space = data.find_last_of(' ', end);
    if (space == std::string::npos) {
        throw std::runtime_error("测试数据格式异常");
    }
    return space;
}

/// 去掉尾部的 AI 档位字段，模拟“升级前写入的旧存档”。
/// 只删字段本身，保留行尾换行，旧版解析器读到 25 格后即 EOF。
std::string strip_ai_level(std::string data) {
    const auto space = last_field_start(data);
    const auto newline = data.find_last_of('\n');
    const auto end = (newline == std::string::npos) ? data.size() : newline;
    data.erase(space, end - space);
    return data;
}

/// 把尾部 AI 档位换成任意文本，用来构造越界值与非法文本两种损坏。
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
        manager.save_game("random", state, ai::Level::kRandom);
        manager.save_game("search", state, ai::Level::kSearch);

        // 1. 档位必须随存档往返，且两种档位各自独立。
        const auto random_saved = manager.load_game("random");
        expect(random_saved.has_value(), "random save is loadable");
        expect(random_saved && random_saved->ai_level == ai::Level::kRandom,
               "random save restores the random AI level");

        const auto search_saved = manager.load_game("search");
        expect(search_saved && search_saved->ai_level == ai::Level::kSearch,
               "search save restores the search AI level");

        // 2. 局面本身不能被档位字段影响。
        expect(random_saved && random_saved->state.phase == state.phase &&
                   random_saved->state.current_dice == state.current_dice,
               "board state still round-trips alongside the AI level");

        // 下面用原始 SQL 伪造旧档与损坏档，因此先把 data 取出来。
        {
            storage::Database db(kDbPath);
            const std::string data = read_data(db, "random");

            // 3. 旧存档没有尾部档位字段：必须读得出来，并落到默认档位。
            write_data(db, "legacy", strip_ai_level(data));
            // 4. 档位越界：视为数据损坏，不能静默降级。
            write_data(db, "bad-level", replace_ai_level(data, "9"));
            // 5. 档位不是整数：同样视为数据损坏。
            write_data(db, "garbage", replace_ai_level(data, "abc"));
        }

        const auto legacy_saved = manager.load_game("legacy");
        expect(legacy_saved && legacy_saved->ai_level == ai::kDefaultLevel,
               "old save without an AI level falls back to the default (latest)");

        bool bad_level_threw = false;
        try {
            (void)manager.load_game("bad-level");
        } catch (const std::runtime_error&) {
            bad_level_threw = true;
        }
        expect(bad_level_threw, "out-of-range AI level is rejected");

        bool garbage_threw = false;
        try {
            (void)manager.load_game("garbage");
        } catch (const std::runtime_error&) {
            garbage_threw = true;
        }
        expect(garbage_threw, "non-numeric AI level field is rejected");
    }

    // 连接全部析构后再删文件，否则 Windows 上会因文件被占用而失败。
    std::remove(kDbPath);

    if (failures == 0) {
        std::cout << "ALL PASS\n";
    } else {
        std::cout << failures << " FAILURE(S)\n";
    }
    return failures == 0 ? 0 : 1;
}
