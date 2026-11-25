#include "botzone_protocol.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

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

// 校验并返回 0/1 阵营值。非整数或越界都按协议错误抛异常：静默当成某一方
// 会让整局棋从一开始就走错颜色，反而不如让上层回空 response。
int check_color(const json& value, const char* field_name) {
    if (!value.is_number_integer()) {
        throw ProtocolError(std::string("Invalid color in ") + field_name);
    }
    int color = kNoPiece;
    try {
        color = value.get<int>();
    } catch (const std::exception&) {
        throw ProtocolError(std::string("Invalid color in ") + field_name);
    }
    if (color != kBotzoneRedPlayer && color != kBotzoneBluePlayer) {
        throw ProtocolError(std::string("Invalid color in ") + field_name);
    }
    return color;
}

// 从最新一帧 request 取本方阵营。裁判在每个 request 的 content 里都带
// color 字段，这是平台上的唯一来源。
//
// 顶层 input["initdata"]["player"] 只是早期本地手工用例的写法（平台从不下发
// 顶层 initdata），保留它仅为兼容旧命令；两者都缺时退回红方，与最早的行为一致。
int resolve_color(const json& latest_req, const json& input) {
    if (latest_req.contains("color")) {
        return check_color(latest_req["color"], "request color");
    }
    if (input.contains("initdata") && input["initdata"].is_object() &&
        input["initdata"].contains("player")) {
        return check_color(input["initdata"]["player"], "initdata.player");
    }
    return kBotzoneRedPlayer;
}

// 把平台下发的二维棋盘解码成 core::Board。
// 协议要求严格的 5x5 整数矩阵：缺行、缺列、非数组、非整数都拒绝。
// 过去会把缺失格当空格、把残缺棋盘直接交给 AI，等于让 AI 在错误局面上决策。
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
            if (!cell.is_number_integer()) {
                throw ProtocolError("Board cells must be integers");
            }

            int val = kNoPiece;
            try {
                val = cell.get<int>();
            } catch (const std::exception&) {
                throw ProtocolError("Board cell is out of range");
            }

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

            // 编码与存档一致：正数红方、负数蓝方、0 空格。
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
    // phase 缺失时按空串处理，最终落入"未知阶段"分支，返回空 response；
    // 未知字符串阶段也保持空 response，与历史行为一致。
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
    const bool is_red = resolve_color(latest_req, input) == kBotzoneRedPlayer;
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = is_red ? core::Side::Red : core::Side::Blue;
    state.phase = is_red ? core::Phase::Player1Thinking : core::Phase::Player2Thinking;

    // 骰子必须显式给出且为 1~6 的整数；缺失、非整数、越界都按协议错误处理。
    if (!latest_req.contains("dice")) {
        throw ProtocolError("Missing dice");
    }
    if (!latest_req["dice"].is_number_integer()) {
        throw ProtocolError("dice must be an integer");
    }
    int dice = kNoPiece;
    try {
        dice = latest_req["dice"].get<int>();
    } catch (const std::exception&) {
        throw ProtocolError("dice is out of range");
    }
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
