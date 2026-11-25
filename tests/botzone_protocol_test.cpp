// Botzone 协议回归：不启动子进程，直接调用 botzone::handle_request / handle_line，
// 验证布阵帧、走子帧、颜色兼容回退，以及所有非法输入的拒绝路径。
//
// 抽成库之前这些逻辑只能通过“喂一行 JSON 给 exe、再解析 stdout”间接覆盖；
// 现在可以精确断言返回的 JSON 结构，并复用 core 的合法着法枚举检查 AI 的落点。

#include <array>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "ai/ai.h"
#include "botzone_protocol.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"

#include "test_support.h"

using json = nlohmann::json;
using test_support::expect;

namespace {

/// @brief 把内部局面编码成协议要求的二维数组：正数红方、负数蓝方、0 空格。
json make_board(const core::GameState& state) {
    json board = json::array();
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        json row = json::array();
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            const auto& piece = state.board.at({x, y});
            int value = 0;
            if (piece.side == core::Side::Red) {
                value = piece.id;
            } else if (piece.side == core::Side::Blue) {
                value = -piece.id;
            }
            row.push_back(value);
        }
        board.push_back(row);
    }
    return board;
}

/// @brief 构造一帧 move 请求；color 为空表示请求里不带 color 字段。
json make_move_request(const core::GameState& state, int dice, std::optional<int> color) {
    json item;
    item["phase"] = "move";
    if (color) {
        item["color"] = *color;
    }
    item["dice"] = dice;
    item["board"] = make_board(state);

    json request;
    request["requests"] = json::array();
    request["requests"].push_back(item);
    return request;
}

/// @brief 红方先手的最小局面：红 1 号在 (0,0)，蓝 1 号在 (4,4)。
core::GameState make_red_state(int dice) {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = dice;
    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({4, 4}) = {core::Side::Blue, 1};
    return state;
}

/// @brief 蓝方行棋的最小局面：蓝 1 号在 (4,4)，红 1 号在 (0,0)。
core::GameState make_blue_state(int dice) {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Blue;
    state.phase = core::Phase::Player2Thinking;
    state.current_dice = dice;
    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({4, 4}) = {core::Side::Blue, 1};
    return state;
}

/// @brief 判断响应里的 layout 是否是 1~6 的一个排列。
bool is_layout_permutation(const json& layout) {
    if (!layout.is_array() || layout.size() != 6) {
        return false;
    }
    std::array<bool, 7> seen{};
    for (const auto& value : layout) {
        if (!value.is_number_integer()) {
            return false;
        }
        const int id = value.get<int>();
        if (id < 1 || id > 6 || seen[static_cast<std::size_t>(id)]) {
            return false;
        }
        seen[static_cast<std::size_t>(id)] = true;
    }
    return true;
}

/// @brief 检查 AI 给出的坐标是不是该局面下的一步合法着法。
bool is_legal_move(const core::GameState& state, int x0, int y0, int x1, int y1) {
    for (const auto& move : core::rules::generate_legal_moves(state)) {
        if (static_cast<int>(move.from.x) == x0 && static_cast<int>(move.from.y) == y0 &&
            static_cast<int>(move.to.x) == x1 && static_cast<int>(move.to.y) == y1) {
            return true;
        }
    }
    return false;
}

/// @brief 协议错误统一用异常报告，测试只关心“有没有抛”。
template <typename Fn>
bool throws_exception(Fn&& fn) {
    try {
        fn();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

/// @brief 非法请求必须抛 ProtocolError，而不是笼统的 std::runtime_error。
template <typename Fn>
bool throws_protocol_error(Fn&& fn) {
    try {
        fn();
    } catch (const botzone::ProtocolError&) {
        return true;
    } catch (const std::exception&) {
        return false;
    }
    return false;
}

/// @brief 从 response 中取出四个坐标，并顺带断言它们都存在且为整数。
struct MoveReply {
    int x0 = -1;
    int y0 = -1;
    int x1 = -1;
    int y1 = -1;
    bool complete = false;
};

MoveReply read_move_reply(const json& response) {
    const json& inner = response.at("response");
    MoveReply reply;
    for (const char* key : {"x0", "y0", "x1", "y1"}) {
        if (!inner.contains(key) || !inner[key].is_number_integer()) {
            return reply;
        }
    }
    reply.x0 = inner["x0"].get<int>();
    reply.y0 = inner["y0"].get<int>();
    reply.x1 = inner["x1"].get<int>();
    reply.y1 = inner["y1"].get<int>();
    reply.complete = true;
    return reply;
}

}  // namespace

int main() {
    // 1. requests 缺失或为空：按首次调用处理，返回 1~6 的布阵排列。
    {
        const json response = botzone::handle_request(json::object());
        expect(is_layout_permutation(response.at("response").at("layout")),
               "missing requests returns a layout permutation");
    }
    {
        json request;
        request["requests"] = json::array();
        const json response = botzone::handle_request(request);
        expect(is_layout_permutation(response.at("response").at("layout")),
               "empty requests returns a layout permutation");
    }

    // 2. 显式布阵帧与空历史走同一分支。
    {
        json request;
        request["requests"] = json::array();
        json item;
        item["phase"] = "layout";
        request["requests"].push_back(item);
        const json response = botzone::handle_request(request);
        expect(is_layout_permutation(response.at("response").at("layout")),
               "layout phase returns a layout permutation");
    }

    // 3. 未知阶段或缺少阶段：返回空 response，而不是崩溃或猜测走法。
    {
        json request;
        request["requests"] = json::array();
        json item;
        item["phase"] = "unknown";
        request["requests"].push_back(item);
        expect(botzone::handle_request(request).at("response").empty(),
               "unknown phase returns an empty response");
    }
    {
        json request;
        request["requests"] = json::array();
        request["requests"].push_back(json::object());
        expect(botzone::handle_request(request).at("response").empty(),
               "missing phase returns an empty response");
    }

    // 4. 红方走子：color=0，返回的起点必须是红 1 号所在的 (0,0)，终点合法。
    {
        const core::GameState state = make_red_state(3);
        const MoveReply reply =
            read_move_reply(botzone::handle_request(make_move_request(state, 3, 0)));
        expect(reply.complete, "red move reply contains all four coordinates");
        expect(reply.x0 == 0 && reply.y0 == 0, "red move starts from the red piece");
        expect(is_legal_move(state, reply.x0, reply.y0, reply.x1, reply.y1),
               "red move is legal for the decoded board");
    }

    // 5. 蓝方走子：color=1，起点必须是蓝 1 号所在的 (4,4)。
    {
        const core::GameState state = make_blue_state(1);
        const MoveReply reply =
            read_move_reply(botzone::handle_request(make_move_request(state, 1, 1)));
        expect(reply.complete, "blue move reply contains all four coordinates");
        expect(reply.x0 == 4 && reply.y0 == 4, "blue move starts from the blue piece");
        expect(is_legal_move(state, reply.x0, reply.y0, reply.x1, reply.y1),
               "blue move is legal for the decoded board");
    }

    // 6. 请求缺 color 时按红方处理；顶层 initdata.player 是旧兼容入口。
    {
        const core::GameState state = make_red_state(1);
        const MoveReply reply =
            read_move_reply(botzone::handle_request(make_move_request(state, 1, std::nullopt)));
        expect(reply.complete && reply.x0 == 0 && reply.y0 == 0,
               "missing color falls back to red");
    }
    {
        const core::GameState state = make_blue_state(1);
        json request = make_move_request(state, 1, std::nullopt);
        request["initdata"] = {{"player", 1}};
        const MoveReply reply = read_move_reply(botzone::handle_request(request));
        expect(reply.complete && reply.x0 == 4 && reply.y0 == 4,
               "initdata.player is honored when the request has no color");
    }

    // 7. 颜色越界或非整数必须拒绝，不能静默挑一方。
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 2);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "out-of-range color is rejected");
    }
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 0);
        request["requests"][0]["color"] = "red";
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-integer color is rejected");
    }

    // 8. 骰子缺失、为 0 或越界都必须拒绝。
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 0);
        request["requests"][0].erase("dice");
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "missing dice is rejected");
    }
    {
        const core::GameState state = make_red_state(0);
        json request = make_move_request(state, 0, 0);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "zero dice is rejected");
    }
    {
        const core::GameState state = make_red_state(7);
        json request = make_move_request(state, 7, 0);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "out-of-range dice is rejected");
    }

    // 9. 棋盘缺失或不是二维数组必须拒绝。
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 0);
        request["requests"][0].erase("board");
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "missing board is rejected");
    }
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 0);
        request["requests"][0]["board"] = 42;
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-array board is rejected");
    }

    // 10. 棋子编号越界或同一方重复必须拒绝；空格与对方棋子不受影响。
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 0);
        request["requests"][0]["board"][0][0] = 7;
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "piece id above six is rejected");
    }
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 0);
        request["requests"][0]["board"][0][1] = 1;
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "duplicate red piece id is rejected");
    }
    {
        const core::GameState state = make_red_state(1);
        json request = make_move_request(state, 1, 0);
        // 两方各有一个 1 号是合法的；只有同一方重复才应拒绝。
        request["requests"][0]["board"][4][4] = -1;
        expect(!throws_exception([&] { (void)botzone::handle_request(request); }),
               "same id on opposite sides is accepted");
    }

    // 11. 入口层：非法 JSON 文本必须抛异常，交给 main 兜底为空 response。
    expect(throws_protocol_error([] { (void)botzone::handle_line("{"); }),
           "malformed JSON line is a protocol error");

    // 12. 协议结构错误：requests/phase/board 的类型与维度必须被拒绝。
    {
        expect(throws_protocol_error([] { (void)botzone::handle_request(json::array()); }),
               "non-object request is rejected");
    }
    {
        json request;
        request["requests"] = 42;
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-array requests is rejected");
    }
    {
        json request;
        request["requests"] = json::array();
        request["requests"].push_back(42);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-object latest request is rejected");
    }
    {
        json item;
        item["phase"] = "layout";
        json request;
        request["requests"] = json::array();
        request["requests"].push_back(42);
        request["requests"].push_back(item);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-object historical request is rejected");
    }
    {
        json item;
        item["phase"] = 5;
        json request;
        request["requests"] = json::array();
        request["requests"].push_back(item);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-string phase is rejected");
    }

    const json empty = botzone::empty_response();
    expect(empty.contains("response") && empty["response"].is_object() && empty["response"].empty(),
           "empty_response is a well-formed empty response");

    const core::GameState board_state = make_red_state(1);
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["board"].erase(request["requests"][0]["board"].begin());
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "board with four rows is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["board"][0].erase(request["requests"][0]["board"][0].begin());
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "board row with four cells is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["board"][0] = 42;
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-array board row is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["board"][0][0] = "red";
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-integer board cell is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        const json extra_row = request["requests"][0]["board"][0];
        request["requests"][0]["board"].push_back(extra_row);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "board with six rows is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["board"][0].push_back(0);
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "board row with six cells is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["board"][0][0] = 999999999999LL;
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "board cell outside int range is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["board"][0][0] = -7;
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "negative piece id outside range is rejected");
    }
    {
        json request = make_move_request(board_state, 1, 0);
        request["requests"][0]["dice"] = "3";
        expect(throws_protocol_error([&] { (void)botzone::handle_request(request); }),
               "non-integer dice is rejected");
    }

    // 13. 单行入口与对象入口应返回完全相同的结果（不含随机布阵，用走子帧验证）。
    {
        const core::GameState state = make_red_state(1);
        const json request = make_move_request(state, 1, 0);
        const json from_object = botzone::handle_request(request);
        const json from_line = botzone::handle_line(request.dump());
        expect(from_object == from_line, "handle_line matches handle_request");
    }

    return test_support::finish();
}
