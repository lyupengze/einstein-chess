#include "save_manager.h"

#include <sstream>
#include <stdexcept>
#include <string>

#include "core/board.h"
#include "core/game_state.h"
#include "core/types.h"
#include "sqlite_wrapper.h"

namespace storage {

namespace {

constexpr int kNoDice = -1;

[[nodiscard]] std::string serialize(const core::GameState& state) {
    std::ostringstream oss;

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
                val = -piece.id;
            }
            oss << val << ' ';
        }
    }
    return oss.str();
}

core::GameState deserialize(const std::string& data) {
    std::istringstream iss(data);
    core::GameState state;

    int p1_side, cur_side, phase, dice, winner, p1_ready, p2_ready;
    if (!(iss >> p1_side >> cur_side >> phase >> dice >> winner >> p1_ready >> p2_ready)) {
        throw std::runtime_error("反序列化失败: 元数据缺失或损坏");
    }

    constexpr int kSideMin = static_cast<int>(core::Side::None);
    constexpr int kSideMax = static_cast<int>(core::Side::Blue);
    constexpr int kPhaseMin = static_cast<int>(core::Phase::LayOut);
    constexpr int kPhaseMax = static_cast<int>(core::Phase::GameOver);
    constexpr int kMaxChessId = core::Board::kChessCount;

    if (p1_side < kSideMin || p1_side > kSideMax || cur_side < kSideMin || cur_side > kSideMax ||
        winner < kSideMin || winner > kSideMax) {
        throw std::runtime_error("反序列化失败: 非法阵营数据");
    }

    if (phase < kPhaseMin || phase > kPhaseMax) {
        throw std::runtime_error("反序列化失败: 非法阶段数据");
    }

    if (dice != kNoDice && (dice < 1 || dice > kMaxChessId)) {
        throw std::runtime_error("反序列化失败: 非法骰子点数");
    }

    if ((p1_ready != 0 && p1_ready != 1) || (p2_ready != 0 && p2_ready != 1)) {
        throw std::runtime_error("反序列化失败: 非法 ready 状态");
    }

    state.player1_side = static_cast<core::Side>(p1_side);
    state.current_side = static_cast<core::Side>(cur_side);
    state.phase = static_cast<core::Phase>(phase);
    state.winner = static_cast<core::Side>(winner);
    state.player1_ready = static_cast<bool>(p1_ready);
    state.player2_ready = static_cast<bool>(p2_ready);

    if (dice != kNoDice) {
        state.current_dice = dice;
    }

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
                state.board.at({x, y}) = {core::Side::Blue, -val};
            } else {
                state.board.at({x, y}) = {core::Side::None, 0};
            }
        }
    }

    std::string extra;
    if (iss >> extra) {
        throw std::runtime_error("反序列化失败: 数据格式中存在冗余内容");
    }

    return state;
}

}  // namespace

SaveManager::SaveManager(const std::string& db_path) : db_(db_path) {
    Statement stmt(db_,
                   "CREATE TABLE IF NOT EXISTS saves (name TEXT PRIMARY KEY, data TEXT NOT NULL, "
                   "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);");
    stmt.step();
}

void SaveManager::save_game(const std::string& slot_name, const core::GameState& state) {
    Statement stmt(db_, "INSERT OR REPLACE INTO saves (name, data) VALUES (?, ?);");
    stmt.bind(1, slot_name);
    stmt.bind(2, serialize(state));
    stmt.step();
}

std::optional<core::GameState> SaveManager::load_game(const std::string& slot_name) {
    Statement stmt(db_, "SELECT data FROM saves WHERE name = ?;");
    stmt.bind(1, slot_name);
    if (stmt.step()) {
        return deserialize(stmt.column_text(0));
    }
    return std::nullopt;
}

std::vector<std::string> SaveManager::list_saves() {
    Statement stmt(db_, "SELECT name FROM saves ORDER BY timestamp DESC;");
    std::vector<std::string> names;
    while (stmt.step()) {
        names.push_back(stmt.column_text(0));
    }
    return names;
}

}  // namespace storage