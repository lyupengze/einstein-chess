#include "save_manager.h"

#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/game_state.h"
#include "core/types.h"
#include "core/validation.h"
#include "sqlite_wrapper.h"

namespace storage {

namespace {

/// 序列化"当前无骰子"的占位值。用 -1 而非 0，以便与合法点数 1~6 区分。
constexpr int kNoDice = -1;

// 序列化格式（单行文本）：
//   7 个局面元数据：player1_side current_side phase dice winner player1_ready player2_ready
//   25 个棋盘格：x（行）为外层、y（列）为内层逐格输出
//   1 个可选元数据：AI 档位（ai::Level 的底层整数值），追加在最后
// 枚举一律按底层整数值写出；dice 无值时写 kNoDice。
// 棋盘格编码：正数 = 红方棋子编号，负数 = 蓝方棋子编号，0 = 空格。
// AI 档位放在末尾而不是开头：旧存档（没有这一项）仍能按“读到 25 格就结束”解析，
// 新存档多出一个尾部整数也不会与任何棋盘格混淆。
[[nodiscard]] std::string serialize(const core::GameState& state, ai::Level level) {
    // 写库前先校验：不允许把"字段都能序列化、组合起来却不可能"的局面存进档案，
    // 否则下次读档时才暴露，排查成本更高。
    const std::vector<core::StateIssue> issues = core::validate_state(state);
    if (!issues.empty()) {
        throw std::runtime_error("保存失败: " + issues.front().field + ": " +
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

// 反序列化只负责“把文本安全地还原成字段”，领域合法性统一交给 core::validate_state。
// 这样阶段、阵营、骰子、重复编号等规则只在 validate_state 中维护一份，避免两处漂移。
// 解析层仍保留转换安全边界：棋盘格范围（防止 -INT_MIN 溢出）、ready 的 int->bool
// 窄化、AI 档位范围，以及字段是否读全/尾随内容等 validate_state 看不到的格式问题。
// AI 档位是可选的尾部字段：旧存档没有它，按默认档位恢复。
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
            int val;
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

    // 第八个字段（AI 档位）是后加的：旧存档读到 25 格后已经是 EOF，
    // 这时保持 kDefaultLevel；只有“字段存在但解析失败”才视为数据损坏。
    ai::Level level = ai::kDefaultLevel;
    int level_int = 0;
    if (iss >> level_int) {
        constexpr int kLevelMin = static_cast<int>(ai::Level::kRandom);
        constexpr int kLevelMax = static_cast<int>(ai::Level::kSearch);
        if (level_int < kLevelMin || level_int > kLevelMax) {
            throw std::runtime_error("反序列化失败: 非法 AI 档位");
        }
        level = static_cast<ai::Level>(level_int);
    } else if (!iss.eof()) {
        // 非数字的尾部内容（例如存档被手工拼接）同样按损坏处理。
        throw std::runtime_error("反序列化失败: 非法 AI 档位");
    }

    // 读完 AI 档位后流必须恰好结束；有多余内容说明数据被拼接或字段错位。
    std::string extra;
    if (iss >> extra) {
        throw std::runtime_error("反序列化失败: 数据格式中存在冗余内容");
    }

    // 字段都还原后，统一交给 validate_state 做阶段状态机校验；
    // 这里不再重复检查“思考阶段与骰子”等规则，避免与 core 中的定义漂移。
    const std::vector<core::StateIssue> issues = core::validate_state(state);
    if (!issues.empty()) {
        throw std::runtime_error("反序列化失败: " + issues.front().field + ": " +
                                 issues.front().message);
    }

    return SavedGame{state, level};
}

}  // namespace

// 建表语句：IF NOT EXISTS 让重复打开同一数据库是幂等的。
// name 做主键：天然保证槽位唯一，配合 INSERT OR REPLACE 实现"同名覆盖"。
// timestamp 仅用于 list_saves 排序，保存时由 SQLite 自动填当前时间。
SaveManager::SaveManager(const std::string& db_path) : db_(db_path) {
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
    // 按保存时间从新到旧。DATETIME 精度到秒，
    // 同一秒内保存的多个槽位之间顺序不保证。
    Statement stmt(db_, "SELECT name FROM saves ORDER BY timestamp DESC;");
    std::vector<std::string> names;
    while (stmt.step()) {
        names.push_back(stmt.column_text(0));
    }
    return names;
}

}  // namespace storage