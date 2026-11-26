/// @file src/storage/save_manager.cpp
/// @brief 存档格式的序列化与反序列化，以及 saves 表的读写。
///
/// 写库前拒绝非法档位与不自洽局面，读档后再交给 `core::validate_state` 统一
/// 校验；领域规则只在 core 维护一份，storage 只守住格式边界。

#include "save_manager.h"

#include <charconv>
#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core/board.h"
#include "core/game_state.h"
#include "core/types.h"
#include "core/validation.h"
#include "sqlite_wrapper.h"

namespace storage {

namespace {

/// @brief “当前无骰子”的占位值；-1 与合法点数 1~6 不重叠。
constexpr int kNoDice = -1;

// 格式的准确定义见 `docs/SAVE_FORMAT.md`：7 个元数据、25 个棋盘格，最后追加
// 必填的 AI 档位。枚举按底层整数值写出，dice 无值写 kNoDice，棋盘格正数红、
// 负数蓝、0 空。档位缺失或非法直接报错，不静默回落到默认档。
[[nodiscard]] std::string serialize(const core::GameState& state, ai::Level level) {
    // 写库前先校验档位与局面：非法档位不能写进数据库，否则会在读档时才暴露，
    // 还可能覆盖掉同名槽位里的好档。
    if (!ai::is_valid_level(level)) {
        throw std::invalid_argument("非法的 AI 档位: " + std::to_string(static_cast<int>(level)));
    }
    if (state.phase == core::Phase::LayOut) {
        throw std::invalid_argument("布阵阶段的对局不能存档");
    }

    const std::vector<core::StateIssue> issues = core::validate_state(state);
    if (!issues.empty()) {
        throw std::invalid_argument("局面状态不合法: " + issues.front().field + ": " +
                                    issues.front().message);
    }

    std::ostringstream oss;

    // 空格分隔；末尾换行只是可读习惯，反序列化按流读取并不依赖它。
    oss << static_cast<int>(state.player1_side) << ' ' << static_cast<int>(state.current_side)
        << ' ' << static_cast<int>(state.phase) << ' ' << state.current_dice.value_or(kNoDice)
        << ' ' << static_cast<int>(state.winner) << ' ' << static_cast<int>(state.player1_ready)
        << ' ' << static_cast<int>(state.player2_ready) << '\n';

    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const auto& piece = state.board.at({x, y});
            int val = 0;
            if (piece.side == core::Side::Red) {
                val = piece.id;
            } else if (piece.side == core::Side::Blue) {
                // 取负号表示蓝方；棋子编号本身始终为正，不会与红方混淆。
                val = -piece.id;
            }
            oss << val << ' ';
        }
    }
    // 末尾追加 AI 档位。棋盘最后一格后面已经有一个空格，这里直接写数字即可分隔。
    oss << static_cast<int>(level) << '\n';
    return oss.str();
}

// 只负责把文本安全地还原成字段，领域合法性交给 core::validate_state，避免两处漂移；
// 这里只守住 validate_state 看不到的格式边界：棋盘格范围（防止 -INT_MIN 溢出）、
// ready 的窄化、档位必填与范围、尾部不能有额外内容。
SavedGame deserialize(const std::string& data) {
    std::istringstream iss(data);
    core::GameState state;

    // 第一步：7 个元数据必须全部读到，缺任何一个都视为数据损坏。
    int p1_side, cur_side, phase, dice, winner, p1_ready, p2_ready;
    if (!(iss >> p1_side >> cur_side >> phase >> dice >> winner >> p1_ready >> p2_ready)) {
        throw std::runtime_error("反序列化失败: 元数据缺失或损坏");
    }

    constexpr int kMaxChessId = core::Board::kChessCount;

    // ready 只接受 0/1。其他整数转 bool 虽然"能转"（非零即真），
    // 但出现这类值本身就说明数据已经损坏。
    if ((p1_ready != 0 && p1_ready != 1) || (p2_ready != 0 && p2_ready != 1)) {
        throw std::runtime_error("反序列化失败: 非法 ready 状态");
    }

    state.player1_side = static_cast<core::Side>(p1_side);
    state.current_side = static_cast<core::Side>(cur_side);
    state.phase = static_cast<core::Phase>(phase);
    state.winner = static_cast<core::Side>(winner);
    state.player1_ready = static_cast<bool>(p1_ready);
    state.player2_ready = static_cast<bool>(p2_ready);

    // kNoDice 还原成"无骰子"（optional 保持为空），而不是写回 -1。
    if (dice != kNoDice) {
        state.current_dice = dice;
    }

    // 逐格读取棋盘。这里只做编码范围检查，不查重也不要求 1~6 齐全；
    // 重复编号、非法棋子等语义问题统一由 validate_state 报告。
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            int val{};
            if (!(iss >> val)) {
                throw std::runtime_error("反序列化失败: 棋盘数据缺失");
            }
            if (val < -kMaxChessId || val > kMaxChessId) {
                throw std::runtime_error("反序列化失败: 非法的棋子编号");
            }

            if (val > 0) {
                state.board.at({x, y}) = {core::Side::Red, val};
            } else if (val < 0) {
                // 蓝方用 -val 还原出正数编号。
                state.board.at({x, y}) = {core::Side::Blue, -val};
            } else {
                state.board.at({x, y}) = {core::Side::None, 0};
            }
        }
    }

    // 第八个字段（AI 档位）必填。取走尾部后交给 from_chars 完整消费：operator>>
    // 在溢出时会同时置 failbit 与 eofbit，半截数字会被误当成 EOF。
    std::string tail;
    if (!std::getline(iss, tail)) {
        throw std::runtime_error("反序列化失败: 缺少 AI 档位");
    }
    const std::size_t first = tail.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        throw std::runtime_error("反序列化失败: 缺少 AI 档位");
    }
    const std::size_t last = tail.find_last_not_of(" \t\r\n");
    const std::string_view token(tail.data() + first, last - first + 1);

    int level_int = 0;
    const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), level_int);
    if (ec != std::errc{} || ptr != token.data() + token.size()) {
        throw std::runtime_error("反序列化失败: 非法 AI 档位");
    }
    const ai::Level level = static_cast<ai::Level>(level_int);
    if (!ai::is_valid_level(level)) {
        throw std::runtime_error("反序列化失败: 非法 AI 档位");
    }

    // 字段都还原后统一交给 validate_state 做状态机校验，这里不重复检查阶段与骰子的关系。
    const std::vector<core::StateIssue> issues = core::validate_state(state);
    if (!issues.empty()) {
        throw std::runtime_error("反序列化失败: " + issues.front().field + ": " +
                                 issues.front().message);
    }

    // 布阵阶段没有可恢复的界面流程：读回后既不能继续布阵，也不能直接走子，
    // 因此统一在存档边界拒绝，而不是让 GUI/CLI 各自处理半截状态。
    if (state.phase == core::Phase::LayOut) {
        throw std::runtime_error("反序列化失败: 布阵阶段的对局不能读档");
    }

    return SavedGame{state, level};
}

}  // namespace

// 建表语句：IF NOT EXISTS 让重复打开同一数据库是幂等的。
// name 做主键：天然保证槽位唯一，配合 INSERT OR REPLACE 实现"同名覆盖"。
// timestamp 仅用于 list_saves 排序，保存时由 SQLite 自动填当前时间。
SaveManager::SaveManager(std::string_view db_path) : db_(std::string(db_path)) {
    Statement stmt(db_,
                   "CREATE TABLE IF NOT EXISTS saves (name TEXT PRIMARY KEY, data TEXT NOT NULL, "
                   "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);");
    stmt.step();
}

void SaveManager::save_game(const std::string& slot_name, const core::GameState& state,
                            ai::Level level) {
    // INSERT OR REPLACE：主键冲突时先删旧行再插入新行，timestamp 也随之刷新。
    Statement stmt(db_, "INSERT OR REPLACE INTO saves (name, data) VALUES (?, ?);");
    stmt.bind(1, slot_name);
    stmt.bind(2, serialize(state, level));
    stmt.step();
}

std::optional<SavedGame> SaveManager::load_game(const std::string& slot_name) {
    // name 是主键，最多命中一行，step() 一次即可判断槽位是否存在。
    Statement stmt(db_, "SELECT data FROM saves WHERE name = ?;");
    stmt.bind(1, slot_name);
    if (stmt.step()) {
        return deserialize(stmt.column_text(0));
    }
    return std::nullopt;
}

std::vector<std::string> SaveManager::list_saves() {
    // 按保存时间从新到旧。DATETIME 精度到秒；同一秒内用名称升序做稳定的
    // 次级排序键，避免 SQLite 扫描顺序让列表每次打开都不同。
    Statement stmt(db_, "SELECT name FROM saves ORDER BY timestamp DESC, name ASC;");
    std::vector<std::string> names;
    while (stmt.step()) {
        names.push_back(stmt.column_text(0));
    }
    return names;
}

}  // namespace storage
