#include "botzone_protocol.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "ai/ai.h"
#include "core/game_state.h"
#include "core/types.h"

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
        throw std::runtime_error(std::string("Invalid color in ") + field_name);
    }
    const int color = value.get<int>();
    if (color != kBotzoneRedPlayer && color != kBotzoneBluePlayer) {
        throw std::runtime_error(std::string("Invalid color in ") + field_name);
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
// 行/列长度不足的部分按空格处理，避免越界访问；但已出现的格子必须是整数，
// 编号越界或同一方重复都会显式拒绝——过去不校验时非法值会被当成“红方 99 号”
// 塞进棋盘，规则找不到该棋子，最终以“AI 无路可走”的空 response 收场，平台只能判负。
void decode_board(const json& board_json, core::GameState& state) {
    std::array<std::array<bool, kMaxPieceId + 1>, 2> seen{};
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            int val = kNoPiece;

            if (board_json.size() > x) {
                if (board_json[x].is_array() && board_json[x].size() > y) {
                    val = board_json[x][y].get<int>();
                }
            }

            const int piece_id = (val < 0) ? -val : val;
            if (piece_id > kMaxPieceId) {
                throw std::runtime_error("Invalid piece id on board");
            }
            if (piece_id != kNoPiece) {
                const std::size_t side_index = (val > 0) ? 0 : 1;
                if (seen[side_index][static_cast<std::size_t>(piece_id)]) {
                    throw std::runtime_error("Duplicate piece id on board");
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

json handle_request(const json& input) {
    // 先放一个空 response：走任何分支都能保证输出是结构合法的 JSON。
    json output;
    output["response"] = json::object();

    // 缺 requests 字段按"空历史"处理。必须用 value() 而不是 operator[]：
    // 后者对缺失键只有 JSON_ASSERT 保护，Debug 构建会直接中止进程。
    json requests = input.value("requests", json::array());

    if (requests.empty()) {
        // 平台正常流程不会走到这里：布阵帧同样带 phase == "layout"。
        // 只有本地手工用例会给空 requests，按布阵回复即可。
        // std::array<int, 6> 会被 nlohmann 直接序列化成 JSON 数组。
        output["response"]["layout"] = ai::decide_layout();
        return output;
    }

    // 只看最后一个 request；更早的历史与本步决策无关。
    const json& latest_req = requests.back();
    // phase 缺失时按空串处理，最终落入"未知阶段"分支，返回空 response。
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
    // current_side 才是真正要决策的一方；phase 设为思考阶段以通过
    // decide_move 的前置检查。
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = (resolve_color(latest_req, input) == kBotzoneRedPlayer)
                             ? core::Side::Red
                             : core::Side::Blue;
    state.phase = core::Phase::Player1Thinking;

    // 骰子缺失时 value() 返回 kNoPiece(0)，随即被范围检查拒绝，
    // 从而与合法点数 1~6 区分开。
    const int dice = latest_req.value("dice", kNoPiece);
    if (dice < kMinDice || dice > kMaxDice) {
        throw std::runtime_error("Missing or invalid dice");
    }
    state.current_dice = dice;

    // board 必须是二维数组；类型不符直接报错，不做隐式猜测。
    if (!latest_req.contains("board") || !latest_req["board"].is_array()) {
        throw std::runtime_error("Missing board");
    }
    decode_board(latest_req["board"], state);

    // 输出四个坐标：x 为行、y 为列，与 core::Pos 的约定一致。
    const core::Move move = ai::decide_move(state);
    output["response"]["x0"] = static_cast<int>(move.from.x);
    output["response"]["y0"] = static_cast<int>(move.from.y);
    output["response"]["x1"] = static_cast<int>(move.to.x);
    output["response"]["y1"] = static_cast<int>(move.to.y);
    return output;
}

json handle_line(const std::string& line) {
    // JSON 语法错误会在这里抛异常，由 main 统一兜底；测试可直接断言异常类型。
    return handle_request(json::parse(line));
}

}  // namespace botzone
