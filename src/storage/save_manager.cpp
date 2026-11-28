/// @file src/storage/save_manager.cpp
/// @brief 存档格式的序列化与反序列化，以及 saves 表的读写。
///
/// 写库前拒绝非法档位与不自洽局面，读档后再交给 `core::validate_state` 统一
/// 校验；领域规则只在 core 维护一份，storage 只守住格式边界。

#include "save_manager.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/board.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/types.h"
#include "core/validation.h"
#include "sqlite_wrapper.h"

namespace storage {

namespace {

/// @brief “当前无骰子”的占位值；-1 与合法点数 1~6 不重叠。
constexpr int kNoDice = -1;

/// @brief 整数 token 的解析结果；区分“不是整数”与“溢出 32 位”两种拒绝原因。
enum class IntParse { Ok, NotInteger, Overflow };

/// @brief 把整段文本解析成 32 位有符号整数，成功时写回 value。
///
/// 比 `operator>>` 严格：必须完整消费整段文本，不接受前导 `+`。失败时流状态
/// 不受影响，调用方可以给出带字段名的错误消息。
[[nodiscard]] IntParse parse_token(std::string_view token, int& value) {
    const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (ec == std::errc::result_out_of_range) {
        return IntParse::Overflow;
    }
    if (ec != std::errc{} || ptr != token.data() + token.size()) {
        return IntParse::NotInteger;
    }
    return IntParse::Ok;
}

/// @brief 判断 token 是否把 0 写成了负数（`-0`、`-00`）。
///
/// 值上等价于 `0`，但写入端只产出 `0`，出现别的写法说明数据被手改过。
[[nodiscard]] bool is_negative_zero(std::string_view token) {
    return token.size() > 1 && token.front() == '-' &&
           token.find_first_not_of('0', 1) == std::string_view::npos;
}

/// @brief 解析一个整数 token，失败按数据损坏抛出；label 用来定位字段或坐标。
int parse_checked(const std::string& token, const std::string& label) {
    if (is_negative_zero(token)) {
        throw std::runtime_error("反序列化失败: " + label + " 不能写成 -0");
    }
    int value{};
    switch (parse_token(token, value)) {
        case IntParse::Overflow:
            throw std::runtime_error("反序列化失败: " + label + " 溢出 32 位整数");
        case IntParse::NotInteger:
            throw std::runtime_error("反序列化失败: " + label + " 不是整数");
        case IntParse::Ok:
            break;
    }
    return value;
}

/// @brief 无符号整数 token 的解析结果；区分“不是整数”与“溢出 64 位”。
enum class UIntParse { Ok, NotInteger, Overflow };

/// @brief 把整段文本解析成 64 位无符号整数，成功时写回 value。
///
/// 无符号解析不接受前导 `-`，因此 `-1` 会落到 NotInteger；负数零另有专门检查，
/// 目的是给出比“不是整数”更准确的拒绝原因。
[[nodiscard]] UIntParse parse_unsigned_token(std::string_view token, std::uint64_t& value) {
    const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (ec == std::errc::result_out_of_range) {
        return UIntParse::Overflow;
    }
    if (ec != std::errc{} || ptr != token.data() + token.size()) {
        return UIntParse::NotInteger;
    }
    return UIntParse::Ok;
}

/// @brief 解析一个无符号整数 token，失败按数据损坏抛出；label 用来定位字段。
std::uint64_t parse_checked_unsigned(const std::string& token, const std::string& label,
                                     std::uint64_t max_value) {
    if (is_negative_zero(token)) {
        throw std::runtime_error("反序列化失败: " + label + " 不能写成 -0");
    }
    std::uint64_t value{};
    switch (parse_unsigned_token(token, value)) {
        case UIntParse::Overflow:
            throw std::runtime_error("反序列化失败: " + label + " 超出 64 位无符号整数");
        case UIntParse::NotInteger:
            throw std::runtime_error("反序列化失败: " + label + " 不是整数");
        case UIntParse::Ok:
            break;
    }
    if (value > max_value) {
        throw std::runtime_error("反序列化失败: " + label + " 超出 0.." +
                                 std::to_string(max_value));
    }
    return value;
}

// 格式的准确定义见 `docs/SAVE_FORMAT.md`：第一行是 10 个标量（7 个元数据、AI 档位、
// 随机种子、已掷次数），第二行恰好是 25 个棋盘格。枚举按底层整数值写出，dice 无值写
// kNoDice，棋盘格正数红、负数蓝、0 空。档位缺失或非法直接报错，不静默回落到默认档。
[[nodiscard]] std::string serialize(const core::GameState& state, ai::Level level,
                                    core::RngState rng) {
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

    // 随机流是读档续玩的一部分：种子或已掷次数超出范围，说明调用方传错了。
    if (rng.rolls > core::kMaxRollCount) {
        throw std::invalid_argument("已掷次数超出 0.." + std::to_string(core::kMaxRollCount));
    }

    std::ostringstream oss;

    // 第一行：标量头部，空格分隔，顺序见 `docs/SAVE_FORMAT.md`。
    oss << static_cast<int>(state.player1_side) << ' ' << static_cast<int>(state.current_side)
        << ' ' << static_cast<int>(state.phase) << ' ' << state.current_dice.value_or(kNoDice)
        << ' ' << static_cast<int>(state.winner) << ' ' << static_cast<int>(state.player1_ready)
        << ' ' << static_cast<int>(state.player2_ready) << ' ' << static_cast<int>(level) << ' '
        << rng.seed << ' ' << rng.rolls << '\n';

    // 第二行：只有棋盘，恰好 25 个整数，格间一个空格、行尾不留空格。
    bool first_cell = true;
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            if (!first_cell) {
                oss << ' ';
            }
            first_cell = false;

            const auto& piece = state.board.at({x, y});
            int val = 0;
            if (piece.side == core::Side::Red) {
                val = piece.id;
            } else if (piece.side == core::Side::Blue) {
                // 取负号表示蓝方；棋子编号本身始终为正，不会与红方混淆。
                val = -piece.id;
            }
            oss << val;
        }
    }
    oss << '\n';
    return oss.str();
}

// 只负责把文本安全地还原成字段，领域合法性交给 core::validate_state，避免两处漂移；
// 这里只守住 validate_state 看不到的格式边界：整数 token 的格式（溢出、负数零）、
// 棋盘格范围（防止 -INT_MIN 溢出）、ready 的窄化、档位/种子/已掷次数的必填与范围，
// 以及两行之后不能有额外内容。
SavedGame deserialize(const std::string& data) {
    std::istringstream iss(data);
    core::GameState state;

    // 第一步：第一行的 10 个标量必须全部到位，且每个都是完整的整数。按行取 token，
    // 缺字段不会吃掉第二行的棋盘格，多出来的字段也能单独报出来。
    constexpr std::array<const char*, 7> kMetadataNames{
        "player1_side", "current_side",  "phase",        "dice",
        "winner",       "player1_ready", "player2_ready"};
    constexpr std::array<const char*, 3> kHeaderTailNames{"AI 档位", "随机种子", "已掷次数"};

    std::string header_line;
    if (!std::getline(iss, header_line)) {
        throw std::runtime_error("反序列化失败: 缺少头部行");
    }
    std::istringstream header_stream(header_line);

    std::array<std::string, kMetadataNames.size()> metadata_tokens;
    for (auto& token : metadata_tokens) {
        if (!(header_stream >> token)) {
            throw std::runtime_error("反序列化失败: 头部字段不足 10 个");
        }
    }
    std::array<std::string, kHeaderTailNames.size()> header_tail_tokens;
    for (std::size_t i = 0; i < header_tail_tokens.size(); ++i) {
        if (!(header_stream >> header_tail_tokens[i])) {
            throw std::runtime_error("反序列化失败: 缺少" + std::string(kHeaderTailNames[i]));
        }
    }
    if (std::string extra; header_stream >> extra) {
        throw std::runtime_error("反序列化失败: 头部字段之后还有额外内容");
    }

    std::array<int, kMetadataNames.size()> metadata{};
    for (std::size_t i = 0; i < metadata.size(); ++i) {
        metadata[i] =
            parse_checked(metadata_tokens[i], std::string("元数据字段 ") + kMetadataNames[i]);
    }
    const int p1_side = metadata[0];
    const int cur_side = metadata[1];
    const int phase = metadata[2];
    const int dice = metadata[3];
    const int winner = metadata[4];
    const int p1_ready = metadata[5];
    const int p2_ready = metadata[6];

    // ready 只接受 0/1。其他整数转 bool 虽然"能转"（非零即真），
    // 但出现这类值本身就说明数据已经损坏。
    if ((p1_ready != 0 && p1_ready != 1) || (p2_ready != 0 && p2_ready != 1)) {
        throw std::runtime_error("反序列化失败: ready 字段不是 0 或 1");
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

    // 头部最后三个标量：AI 档位、随机种子、已掷次数，三个都必填。
    const int level_int = parse_checked(header_tail_tokens[0], "AI 档位");
    const ai::Level level = static_cast<ai::Level>(level_int);
    if (!ai::is_valid_level(level)) {
        throw std::runtime_error("反序列化失败: AI 档位 " + std::to_string(level_int) +
                                 " 不在可选范围内");
    }
    const std::uint32_t seed = static_cast<std::uint32_t>(parse_checked_unsigned(
        header_tail_tokens[1], "随机种子", std::numeric_limits<std::uint32_t>::max()));
    const std::uint64_t rolls =
        parse_checked_unsigned(header_tail_tokens[2], "已掷次数", core::kMaxRollCount);

    // 第二步：第二行必须是完整的 25 个格子，多一个少一个都按损坏拒绝。
    constexpr int kMaxChessId = core::Board::kChessCount;
    std::string board_line;
    if (!std::getline(iss, board_line)) {
        throw std::runtime_error("反序列化失败: 缺少棋盘行");
    }
    std::istringstream board_stream(board_line);
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const std::string label =
                "棋盘格 (" + std::to_string(x) + ", " + std::to_string(y) + ")";
            std::string token;
            if (!(board_stream >> token)) {
                throw std::runtime_error("反序列化失败: " + label + " 缺失");
            }
            const int val = parse_checked(token, label);
            if (val < -kMaxChessId || val > kMaxChessId) {
                throw std::runtime_error("反序列化失败: " + label + " 的编号超出 -" +
                                         std::to_string(kMaxChessId) + ".." +
                                         std::to_string(kMaxChessId));
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
    if (std::string extra; board_stream >> extra) {
        throw std::runtime_error("反序列化失败: 棋盘行还有额外内容");
    }
    if (std::string extra_line; iss >> extra_line) {
        throw std::runtime_error("反序列化失败: 棋盘行之后还有额外内容");
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

    return SavedGame{state, level, core::RngState{seed, rolls}};
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
                            ai::Level level, core::RngState rng) {
    // INSERT OR REPLACE：主键冲突时先删旧行再插入新行，timestamp 也随之刷新。
    Statement stmt(db_, "INSERT OR REPLACE INTO saves (name, data) VALUES (?, ?);");
    stmt.bind(1, slot_name);
    stmt.bind(2, serialize(state, level, rng));
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
