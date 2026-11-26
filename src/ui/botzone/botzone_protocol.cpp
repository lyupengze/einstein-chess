/// @file src/ui/botzone/botzone_protocol.cpp
/// @brief 解析 Botzone 请求、重建局面并调用 AI 决策。
///
/// 所有字段都做显式类型与范围检查：协议层宁可拒绝一帧，也不把截断后的
/// 整数或残缺棋盘交给 AI。

#include "botzone_protocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "ai/ai.h"
#include "core/game_state.h"
#include "core/types.h"
#include "core/validation.h"

namespace botzone {

namespace {

using json = nlohmann::json;

// 阵营取值约定（裁判在 request 的 color 字段里给出）：0 = 红方（先手），1 = 蓝方。
constexpr int kBotzoneRedPlayer = 0;
constexpr int kBotzoneBluePlayer = 1;
// 棋盘格编码：正数 = 红方棋子编号，负数 = 蓝方棋子编号，0 = 空格。
constexpr int kNoPiece = 0;
constexpr int kMinDice = 1;
constexpr int kMaxDice = 6;
// 棋子编号只有 1~6；超出范围的值不是“强一点的棋子”，而是协议错误。
constexpr int kMaxPieceId = core::Board::kChessCount;

// 把 JSON 数值严格转换成 int。
//
// nlohmann::json::get<int>() 会在 64 位整数超出 int 时静默截断，例如
// 4294967297 会变成 1，足以让棋子编号/骰子/阵营绕过后续范围检查。
// 这里先按无符号/有符号 64 位读取，再做一次显式范围检查，确保协议层
// 不会把一个“外面看起来很大”的值悄悄解释成合法的小整数。
int checked_json_int(const json& value, std::string_view field_name) {
    const auto invalid = [field_name]() {
        return ProtocolError("字段 " + std::string(field_name) + " 必须是 32 位有符号整数");
    };

    if (value.is_number_unsigned()) {
        const std::uint64_t raw = value.get<std::uint64_t>();
        if (raw > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            throw invalid();
        }
        return static_cast<int>(raw);
    }

    if (value.is_number_integer()) {
        const std::int64_t raw = value.get<std::int64_t>();
        if (raw < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
            raw > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            throw invalid();
        }
        return static_cast<int>(raw);
    }

    throw invalid();
}

// 校验并返回 0/1 阵营值。非整数或越界都按协议错误抛异常：静默当成某一方
// 会让整局棋从一开始就走错颜色，反而不如让上层回空 response。
int check_color(const json& value, std::string_view field_name) {
    const int color = checked_json_int(value, field_name);
    if (color != kBotzoneRedPlayer && color != kBotzoneBluePlayer) {
        throw ProtocolError("Invalid color in " + std::string(field_name));
    }
    return color;
}

// 从最新一帧 request 取本方阵营。裁判在每个 request 里都带 color 字段，
// 这是平台上的唯一来源；缺字段或类型错误都必须拒绝，不能猜成红方。
int resolve_color(const json& latest_req) {
    if (!latest_req.contains("color")) {
        throw ProtocolError("Missing color");
    }
    return check_color(latest_req["color"], "request color");
}

// 把平台下发的二维棋盘解码成 core::Board。协议要求严格的 5x5 整数矩阵：
// 缺行、缺列、非数组、非整数都拒绝，避免 AI 在残缺局面上决策。
void decode_board(const json& board_json, core::GameState& state) {
    if (!board_json.is_array() || board_json.size() != core::Board::kSize) {
        throw ProtocolError("Board must contain exactly 5 rows");
    }

    std::array<std::array<bool, kMaxPieceId + 1>, 2> seen{};
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        const json& row = board_json[x];
        if (!row.is_array() || row.size() != core::Board::kSize) {
            throw ProtocolError("Each board row must contain exactly 5 cells");
        }

        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const json& cell = row[y];
            int val = checked_json_int(cell, "board cell");

            // 先做范围检查再取绝对值，避免 INT_MIN 取负溢出。
            if (val < -kMaxPieceId || val > kMaxPieceId) {
                throw ProtocolError("Invalid piece id on board");
            }

            const int piece_id = (val < 0) ? -val : val;
            if (piece_id != kNoPiece) {
                const std::size_t side_index = (val > 0) ? 0 : 1;
                if (seen[side_index][static_cast<std::size_t>(piece_id)]) {
                    throw ProtocolError("Duplicate piece id on board");
                }
                seen[side_index][static_cast<std::size_t>(piece_id)] = true;
            }

            if (val > kNoPiece) {
                state.board.at({x, y}) = {core::Side::Red, val};
            } else if (val < kNoPiece) {
                state.board.at({x, y}) = {core::Side::Blue, -val};
            } else {
                state.board.at({x, y}) = {core::Side::None, kNoPiece};
            }
        }
    }
}

}  // namespace

json empty_response() {
    return json{{"response", json::object()}};
}

json handle_request(const json& input) {
    if (!input.is_object()) {
        throw ProtocolError("Request must be a JSON object");
    }

    // 先放一个空 response：走任何分支都能保证输出是结构合法的 JSON。
    json output = empty_response();

    // 缺 requests 字段按"空历史"处理。必须显式判断类型：value() 不会拒绝
    // requests 是对象/数字等协议错误，后面 empty()/back() 的行为会变得不可控。
    json requests = json::array();
    if (input.contains("requests")) {
        if (!input["requests"].is_array()) {
            throw ProtocolError("requests must be an array");
        }
        requests = input["requests"];
    }

    if (requests.empty()) {
        // 平台正常流程不会走到这里：布阵帧同样带 phase == "layout"。
        // 只有本地手工用例会给空 requests，按布阵回复即可。
        // std::array<int, 6> 会被 nlohmann 直接序列化成 JSON 数组。
        output["response"]["layout"] = ai::decide_layout();
        return output;
    }

    // 历史里混入非对象会让后续按字段读取的行为不可控，整帧直接拒绝。
    for (const auto& item : requests) {
        if (!item.is_object()) {
            throw ProtocolError("Each request must be a JSON object");
        }
    }

    // 只看最后一个 request；更早的历史与本步决策无关。
    const json& latest_req = requests.back();
    if (latest_req.contains("phase") && !latest_req["phase"].is_string()) {
        throw ProtocolError("phase must be a string");
    }
    // phase 缺失时按空串处理，与无法识别的阶段一样返回空 response。
    const std::string phase = latest_req.value("phase", "");

    if (phase == "layout") {
        // 平台要求重新布阵：与首次调用相同的响应。
        output["response"]["layout"] = ai::decide_layout();
        return output;
    }

    if (phase != "move") {
        return output;
    }

    // 协议只给"颜色 + 棋盘 + 骰子"，这里拼出 ai::decide_move 所需的最小
    // GameState：player1_side 固定写红方（decide_move 并不关心它），
    // current_side 才是真正要决策的一方。phase 必须与当前方一致，不能一律
    // 写成 Player1Thinking，否则蓝方会构造出自相矛盾的思考阶段。
    const bool is_red = resolve_color(latest_req) == kBotzoneRedPlayer;
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = is_red ? core::Side::Red : core::Side::Blue;
    state.phase = is_red ? core::Phase::Player1Thinking : core::Phase::Player2Thinking;

    // 骰子必须显式给出且为 1~6 的整数；缺失、非整数、越界都按协议错误处理。
    if (!latest_req.contains("dice")) {
        throw ProtocolError("Missing dice");
    }
    int dice = checked_json_int(latest_req["dice"], "dice");
    if (dice < kMinDice || dice > kMaxDice) {
        throw ProtocolError("dice must be between 1 and 6");
    }
    state.current_dice = dice;

    // board 必须是严格的 5x5 整数矩阵；类型/维度不符直接报错，不做隐式猜测。
    if (!latest_req.contains("board")) {
        throw ProtocolError("Missing board");
    }
    if (!latest_req["board"].is_array()) {
        throw ProtocolError("board must be an array");
    }
    decode_board(latest_req["board"], state);

    // 棋盘只校验了维度与单格编码，整体是否自洽还要交给 core 的统一校验：
    // 例如只剩一方棋子、已经占角却仍处于思考阶段，都不应进入 AI 决策。
    const auto issues = core::validate_state(state);
    if (!issues.empty()) {
        std::string detail;
        for (const auto& issue : issues) {
            if (!detail.empty()) {
                detail += "；";
            }
            detail += issue.field + ": " + issue.message;
        }
        throw ProtocolError("局面状态不合法：" + detail);
    }

    // 输出四个坐标：x 为行、y 为列，与 core::Pos 的约定一致。
    const core::Move move = ai::decide_move(state);
    output["response"]["x0"] = static_cast<int>(move.from.x);
    output["response"]["y0"] = static_cast<int>(move.from.y);
    output["response"]["x1"] = static_cast<int>(move.to.x);
    output["response"]["y1"] = static_cast<int>(move.to.y);
    return output;
}

json handle_line(const std::string& line) {
    // JSON 语法错误属于协议错误，统一转成 ProtocolError；AI 内部错误保持原类型。
    try {
        return handle_request(json::parse(line));
    } catch (const ProtocolError&) {
        throw;
    } catch (const json::exception& e) {
        throw ProtocolError(std::string("Invalid Botzone JSON: ") + e.what());
    }
}

}  // namespace botzone
