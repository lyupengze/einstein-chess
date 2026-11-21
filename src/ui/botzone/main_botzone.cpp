// Botzone 无状态适配层：每次进程启动只处理一行 JSON 请求，输出一行 JSON 响应后退出。
// 平台侧保存对局历史，本程序不持有任何跨请求状态——所需信息全部从本次请求重建。
// 输入/输出协议（详见 README 的 Botzone 一节）：
//   - requests 为空：首次调用，返回 response.layout 布阵；
//   - 最后一个 request 的 phase == "layout"：再次布阵；
//   - 最后一个 request 的 phase == "move"：读取 color、棋盘与骰子，返回 x0/y0/x1/y1 走法。
// 注意：本方阵营来自 request 的 color 字段，而不是顶层 initdata——平台不会给 Bot 下发 initdata。

#include <cstddef>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#include "ai/ai.h"
#include "core/game_state.h"
#include "core/types.h"

using json = nlohmann::json;

namespace {

// 阵营取值约定（裁判在 request 的 color 字段里给出）：0 = 红方（先手），1 = 蓝方。
constexpr int kBotzoneRedPlayer = 0;
constexpr int kBotzoneBluePlayer = 1;
// 棋盘格编码：正数 = 红方棋子编号，负数 = 蓝方棋子编号，0 = 空格。
constexpr int kNoPiece = 0;
constexpr int kMinDice = 1;
constexpr int kMaxDice = 6;

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

}  // namespace

int main() {
    // 平台每次调用通过 stdin 给一行 JSON；读不到（EOF）就安静退出。
    std::string inputLine;
    if (!std::getline(std::cin, inputLine)) {
        return 0;
    }

    try {
        // JSON 语法错误会在这里抛异常，由下面的 catch 统一兜底。
        json input = json::parse(inputLine);

        // 先放一个空 response：走任何分支都能保证输出是结构合法的 JSON。
        json output;
        output["response"] = json::object();

        // 缺 requests 字段按"空历史"处理。必须用 value() 而不是 operator[]：
        // 后者对缺失键只有 JSON_ASSERT 保护，Debug 构建会直接中止进程。
        json requests = input.value("requests", json::array());

        if (requests.empty()) {
            // 平台正常流程不会走到这里：布阵帧同样带 phase == "layout"。
            // 只有本地手工用例会给空 requests，按布阵回复即可。
            auto layout = ai::decide_layout();
            // std::array<int, 6> 会被 nlohmann 直接序列化成 JSON 数组。
            output["response"]["layout"] = layout;
        } else {
            // 只看最后一个 request；更早的历史与本步决策无关。
            const json& latest_req = requests.back();
            // phase 缺失时按空串处理，最终落入"未知阶段"分支，返回空 response。
            std::string phase = latest_req.value("phase", "");

            if (phase == "layout") {
                // 平台要求重新布阵：与首次调用相同的响应。
                auto layout = ai::decide_layout();
                output["response"]["layout"] = layout;
            } else if (phase == "move") {
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
                int dice = latest_req.value("dice", kNoPiece);
                if (dice < kMinDice || dice > kMaxDice) {
                    throw std::runtime_error("Missing or invalid dice");
                }
                state.current_dice = dice;

                // board 必须是二维数组；类型不符直接报错，不做隐式猜测。
                if (!latest_req.contains("board") || !latest_req["board"].is_array()) {
                    throw std::runtime_error("Missing board");
                }
                const json& board_json = latest_req["board"];

                // 逐格读入；行/列长度不足的部分按空格处理，避免越界访问。
                // 非整数的单元格会在 get<int>() 处抛异常，由外层统一兜底。
                for (std::size_t x = 0; x < core::Board::kSize; ++x) {
                    for (std::size_t y = 0; y < core::Board::kSize; ++y) {
                        int val = kNoPiece;

                        if (board_json.size() > x) {
                            if (board_json[x].is_array() && board_json[x].size() > y) {
                                val = board_json[x][y].get<int>();
                            }
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

                // 输出四个坐标：x 为行、y 为列，与 core::Pos 的约定一致。
                ai::Move move = ai::decide_move(state);
                output["response"]["x0"] = static_cast<int>(move.from.x);
                output["response"]["y0"] = static_cast<int>(move.from.y);
                output["response"]["x1"] = static_cast<int>(move.to.x);
                output["response"]["y1"] = static_cast<int>(move.to.y);
            }
        }

        // 一行紧凑 JSON + 换行，构成完整的一帧响应。
        std::cout << output.dump() << std::endl;

    } catch (const std::exception& e) {
        // 兜底：任何异常（JSON 语法、字段缺失、无路可走等）都回空 response，
        // 并以 0 退出，让平台看到"本次没有决策"而不是进程崩溃。
        std::cerr << "Bot error: " << e.what() << std::endl;
        std::cout << "{\"response\":{}}" << std::endl;
    }

    return 0;
}