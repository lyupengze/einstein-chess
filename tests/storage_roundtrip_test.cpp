/// @file tests/storage_roundtrip_test.cpp
/// @brief 存档往返回归：全部局面字段经序列化往返后逐字段一致。
///
/// 覆盖同名覆盖、存档列表、跨实例持久化，以及“字段各自合法但组合不自洽”的
/// 损坏数据被拒绝。与 save_level_test 的分工：那个用例关注档位必填与严格解析，
/// 本用例关注局面本身的完整往返、存档边界校验与布阵阶段存档的拒绝。两者都
/// 使用临时 SQLite 文件，互不共享数据库。

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <exception>
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

/// @brief 测试专用临时数据库；工作目录由 CTest 固定到 build/tests/storage_roundtrip。
constexpr const char* kDbPath = "test_storage_roundtrip.db";
/// @brief 专门验证同一秒存档排序的临时数据库，避免污染主夹具。
constexpr const char* kSortDbPath = "test_storage_roundtrip_sort.db";

/// @brief 逐字段比较两个局面，包括 5x5 棋盘的每一格。
/// @note current_dice 是 optional，直接比较即可同时覆盖“有骰子/无骰子”。
bool same_state(const core::GameState& lhs, const core::GameState& rhs) {
    if (lhs.player1_side != rhs.player1_side || lhs.current_side != rhs.current_side ||
        lhs.phase != rhs.phase || lhs.current_dice != rhs.current_dice ||
        lhs.winner != rhs.winner || lhs.player1_ready != rhs.player1_ready ||
        lhs.player2_ready != rhs.player2_ready) {
        return false;
    }

    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const auto& a = lhs.board.at({x, y});
            const auto& b = rhs.board.at({x, y});
            if (a.side != b.side || a.id != b.id) {
                return false;
            }
        }
    }
    return true;
}

/// @brief 满盘局面：红蓝双方各 6 枚棋子都在初始格，用来同时验证正负号编码。
core::GameState make_full_state() {
    core::GameState state;
    state.player1_side = core::Side::Blue;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player2Thinking;
    state.current_dice = 6;
    state.winner = core::Side::None;
    state.player1_ready = true;
    state.player2_ready = true;

    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({0, 1}) = {core::Side::Red, 2};
    state.board.at({0, 2}) = {core::Side::Red, 3};
    state.board.at({1, 0}) = {core::Side::Red, 4};
    state.board.at({1, 1}) = {core::Side::Red, 5};
    state.board.at({2, 0}) = {core::Side::Red, 6};

    state.board.at({4, 4}) = {core::Side::Blue, 1};
    state.board.at({4, 3}) = {core::Side::Blue, 2};
    state.board.at({4, 2}) = {core::Side::Blue, 3};
    state.board.at({3, 4}) = {core::Side::Blue, 4};
    state.board.at({3, 3}) = {core::Side::Blue, 5};
    state.board.at({2, 4}) = {core::Side::Blue, 6};
    return state;
}

/// @brief 布阵阶段：无骰子、双方 ready 不一致、棋盘只有部分棋子。
core::GameState make_layout_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::None;
    state.phase = core::Phase::LayOut;
    state.winner = core::Side::None;
    state.player1_ready = false;
    state.player2_ready = true;

    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({1, 1}) = {core::Side::Red, 2};
    state.board.at({4, 4}) = {core::Side::Blue, 1};
    return state;
}

/// @brief 终局：蓝方占角获胜、无骰子、双方 ready 均为真。
core::GameState make_game_over_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::None;
    state.phase = core::Phase::GameOver;
    state.winner = core::Side::Blue;
    state.player1_ready = true;
    state.player2_ready = true;

    state.board.at({0, 0}) = {core::Side::Blue, 1};
    state.board.at({0, 1}) = {core::Side::Red, 2};
    state.board.at({4, 4}) = {core::Side::Blue, 3};
    return state;
}

/// @brief 把原始序列化文本写进指定槽位，用于伪造反序列化失败的数据。
void write_raw(const std::string& slot, const std::string& data) {
    storage::Database db(kDbPath);
    storage::Statement stmt(db, "INSERT OR REPLACE INTO saves (name, data) VALUES (?, ?);");
    stmt.bind(1, slot);
    stmt.bind(2, data);
    stmt.step();
}

/// @brief 读档是否抛出 std::runtime_error（损坏数据统一用这个异常报告）。
bool load_throws(storage::SaveManager& manager, const std::string& slot) {
    try {
        [[maybe_unused]] const auto& ignored = manager.load_game(slot);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    // 上一轮异常退出可能留下同名文件，先清掉保证从空库开始。
    std::remove(kDbPath);

    {
        storage::SaveManager manager(kDbPath);

        const core::GameState full = make_full_state();
        const core::GameState layout = make_layout_state();
        const core::GameState game_over = make_game_over_state();

        manager.save_game("完整对局", full, ai::Level::Random, core::RngState{});
        manager.save_game("终局", game_over, ai::kDefaultLevel, core::RngState{});

        // 1. 两种可恢复阶段各自完整往返，棋盘每一格与可选骰子都要一致。
        const auto full_saved = manager.load_game("完整对局");
        expect(full_saved.has_value(), "完整存档可读");
        expect(full_saved && same_state(full_saved->state, full), "完整存档的棋盘与骰子逐格往返");
        expect(full_saved && full_saved->ai_level == ai::Level::Random, "完整存档恢复档位");

        const auto over_saved = manager.load_game("终局");
        expect(over_saved && same_state(over_saved->state, game_over), "终局存档往返胜者与空骰子");
        expect(over_saved && over_saved->ai_level == ai::kDefaultLevel, "终局存档恢复默认档");

        // 2. 同名保存必须覆盖旧局面，而不是新增一行或保留旧数据。
        manager.save_game("完整对局", game_over, ai::Level::Search, core::RngState{});
        const auto overwritten = manager.load_game("完整对局");
        expect(overwritten && same_state(overwritten->state, game_over), "同名存档覆盖旧局面");
        expect(overwritten && overwritten->ai_level == ai::Level::Search,
               "覆盖同名存档同时更新档位");

        // 3. 列表应恰好有两个槽位；同名覆盖不能产生重复项。
        const auto names = manager.list_saves();
        expect(names.size() == 2, "list_saves 恰好返回两个槽位");
        expect(std::find(names.begin(), names.end(), "完整对局") != names.end(),
               "列表包含被覆盖的槽位");
        expect(std::find(names.begin(), names.end(), "终局") != names.end(), "列表包含终局槽位");
        expect(std::count(names.begin(), names.end(), "完整对局") == 1, "覆盖后的槽位不重复");

        // 4. 不存在的槽位返回空 optional，而不是抛异常。
        expect(!manager.load_game("不存在的档位").has_value(), "不存在的槽位返回 nullopt");

        // 5. 保存前也要校验：不能把"能序列化但语义不自洽"的局面写进数据库。
        //    这里复用满盘局面，只把发起方写成 None；若 serialize 没有校验，
        //    下一次 load_game 才会失败，问题被推迟到读档时。
        core::GameState invalid = full;
        invalid.player1_side = core::Side::None;
        bool save_threw = false;
        try {
            manager.save_game("非法局面", invalid, ai::kDefaultLevel, core::RngState{});
        } catch (const std::invalid_argument&) {
            save_threw = true;
        }
        expect(save_threw, "save_game 拒绝语义非法的局面");

        // 6. 布阵阶段没有可恢复的 UI 流程，必须在写库前拒绝。
        bool layout_save_threw = false;
        try {
            manager.save_game("布阵阶段", layout, ai::Level::Search, core::RngState{});
        } catch (const std::invalid_argument&) {
            layout_save_threw = true;
        }
        expect(layout_save_threw, "save_game 拒绝布阵阶段局面");

        // 7. 非法 AI 档位不能写进库，也不能覆盖同名槽位。
        bool invalid_level_threw = false;
        try {
            manager.save_game("终局", game_over, static_cast<ai::Level>(99), core::RngState{});
        } catch (const std::invalid_argument&) {
            invalid_level_threw = true;
        }
        expect(invalid_level_threw, "save_game 拒绝未知档位");
        const auto preserved = manager.load_game("终局");
        expect(preserved && same_state(preserved->state, game_over) &&
                   preserved->ai_level == ai::kDefaultLevel,
               "非法档位不覆盖已有槽位");

        // 8. 非法局面、布阵阶段、非法档位被拒绝后，列表数量保持不变。
        expect(manager.list_saves().size() == 2, "被拒绝的保存不新增槽位");
    }

    // 9. 关闭连接后重新打开同一个数据库，数据仍应完整存在（真正的持久化）。
    {
        storage::SaveManager reopened(kDbPath);
        const core::GameState game_over = make_game_over_state();
        const auto saved = reopened.load_game("完整对局");
        expect(saved && same_state(saved->state, game_over), "重开数据库后局面仍在");
        expect(saved && saved->ai_level == ai::Level::Search, "重开数据库后档位仍在");
    }

    // 10. 语义不自洽的数据：每个字段单独看都合法，但组合起来不可能出现。
    //     思考阶段缺骰子会让状态自相矛盾，这类数据必须在读档时拒绝；成因见
    //     `#save-unchecked-state`。
    const std::string empty_board = [] {
        const int cell_count = static_cast<int>(core::Board::kSize * core::Board::kSize);
        std::string cells;
        for (int i = 0; i < cell_count; ++i) {
            cells += "0 ";
        }
        return cells;
    }();

    // 一枚红子 (0,0) + 一枚蓝子 (4,4) 的棋盘，用于构造"字段合法但阶段组合错误"的数据。
    const std::string two_piece_board = [] {
        const int cell_count = static_cast<int>(core::Board::kSize * core::Board::kSize);
        std::string cells;
        for (int i = 0; i < cell_count; ++i) {
            if (i == 0) {
                cells += "1 ";
            } else if (i == cell_count - 1) {
                cells += "-1 ";
            } else {
                cells += "0 ";
            }
        }
        return cells;
    }();

    //    棋盘格写成溢出 32 位的整数：必须按损坏拒绝，不能截断、也不能当成缺失。
    const std::string overflow_cell_board = [] {
        const int cell_count = static_cast<int>(core::Board::kSize * core::Board::kSize);
        std::string cells = "99999999999999999999 ";
        for (int i = 1; i < cell_count; ++i) {
            cells += "0 ";
        }
        return cells;
    }();

    //    棋盘格把空格写成 -0：值与 0 相同，但写入端只产出 0。
    const std::string negative_zero_board = [] {
        const int cell_count = static_cast<int>(core::Board::kSize * core::Board::kSize);
        std::string cells = "-0 ";
        for (int i = 1; i < cell_count; ++i) {
            cells += "0 ";
        }
        return cells;
    }();

    //    思考阶段却写 -1（无骰子）。
    write_raw("缺少骰子", "0 0 2 -1 0 1 1 " + empty_board + "1\n");
    //    掷骰阶段却带着点数 3。
    write_raw("多余骰子", "0 0 1 3 0 1 1 " + empty_board + "1\n");
    //    棋盘只写了两格就结束。
    write_raw("棋盘残缺", "0 0 0 -1 0 0 0 1 2\n");
    //    Player2Thinking 却轮到 player1_side：GUI 会显示人类可走但点击被忽略。
    write_raw("思考方错位", "1 1 3 1 0 1 1 " + two_piece_board + "1\n");
    //    current_side=None 却处于思考阶段：CLI 会在 decide_move 抛异常。
    write_raw("当前方缺失", "1 0 3 1 0 1 1 " + two_piece_board + "1\n");
    //    GameOver 却没有胜者：界面会显示无胜者。
    write_raw("终局无胜者", "1 0 4 -1 0 1 1 " + two_piece_board + "1\n");
    //    布阵阶段局面本身合法，但读档后没有可恢复的界面流程，必须在边界拒绝。
    write_raw("布阵阶段", "1 0 0 -1 0 0 1 " + two_piece_board + "1\n");
    //    整数格式越界：元数据溢出、棋盘格溢出、棋盘格写成 -0、档位写成 -0。
    write_raw("元数据溢出", "99999999999999999999 1 3 1 0 1 1 " + two_piece_board + "1\n");
    write_raw("格子溢出", "1 1 3 1 0 1 1 " + overflow_cell_board + "1\n");
    write_raw("格子负数零", "1 1 3 1 0 1 1 " + negative_zero_board + "1\n");
    write_raw("档位负数零", "1 1 3 1 0 1 1 " + two_piece_board + "-0\n");
    //    档位之后还有第三行：数据只有两行，多出来的行按损坏拒绝。
    write_raw("档位后多出内容", "1 1 3 1 0 1 1 " + two_piece_board + "1\n0\n");

    {
        storage::SaveManager manager(kDbPath);
        expect(load_throws(manager, "缺少骰子"), "思考阶段缺骰子被拒绝");
        expect(load_throws(manager, "多余骰子"), "非思考阶段带骰子被拒绝");
        expect(load_throws(manager, "棋盘残缺"), "棋盘数据残缺被拒绝");
        expect(load_throws(manager, "思考方错位"), "Player2Thinking 轮到我方被拒绝");
        expect(load_throws(manager, "当前方缺失"), "思考阶段当前方为 None 被拒绝");
        expect(load_throws(manager, "终局无胜者"), "终局缺胜者被拒绝");
        expect(load_throws(manager, "布阵阶段"), "布阵阶段存档被拒绝");
        expect(load_throws(manager, "元数据溢出"), "元数据溢出 32 位被拒绝");
        expect(load_throws(manager, "格子溢出"), "棋盘格溢出 32 位被拒绝");
        expect(load_throws(manager, "格子负数零"), "棋盘格写成 -0 被拒绝");
        expect(load_throws(manager, "档位负数零"), "档位写成 -0 被拒绝");
        expect(load_throws(manager, "档位后多出内容"), "档位之后多出内容被拒绝");
    }

    // 连接全部析构后再删文件，否则 Windows 上会因文件被占用而失败。
    std::remove(kDbPath);

    // 8. 同一秒保存多个槽位时，名称必须作为稳定的次级排序键。
    std::remove(kSortDbPath);
    {
        storage::SaveManager manager(kSortDbPath);
        const core::GameState state = make_full_state();
        manager.save_game("beta", state, ai::kDefaultLevel, core::RngState{});
        manager.save_game("alpha", state, ai::kDefaultLevel, core::RngState{});
    }
    {
        storage::Database db(kSortDbPath);
        storage::Statement stmt(db, "UPDATE saves SET timestamp = '2026-01-01 00:00:00';");
        stmt.step();
    }
    {
        storage::SaveManager manager(kSortDbPath);
        const auto names = manager.list_saves();
        expect(names.size() == 2 && names[0] == "alpha" && names[1] == "beta",
               "同一秒的存档按名称排序");
    }
    std::remove(kSortDbPath);

    return test_support::finish();
}
