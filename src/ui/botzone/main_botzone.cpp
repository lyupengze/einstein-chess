#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "ai/ai.h"
#include "core/game_state.h"
#include "core/types.h"

using json = nlohmann::json;

namespace {

constexpr int kBotzoneRedPlayer = 0;
constexpr int kBotzoneBluePlayer = 1;
constexpr int kNoPiece = 0;
constexpr int kMinDice = 1;
constexpr int kMaxDice = 6;

}  // namespace

int main() {
    std::string inputLine;
    if (!std::getline(std::cin, inputLine)) {
        return 0;
    }

    try {
        json input = json::parse(inputLine);

        int my_color = kBotzoneRedPlayer;
        if (input.contains("initdata") && input["initdata"].contains("player")) {
            my_color = input["initdata"]["player"].get<int>();
        }

        json output;
        output["response"] = json::object();

        json requests = input.value("requests", json::array());

        if (requests.empty()) {
            auto layout = ai::decide_layout();
            output["response"]["layout"] = layout;
        } else {
            const json& latest_req = requests.back();
            std::string phase = latest_req.value("phase", "");

            if (phase == "layout") {
                auto layout = ai::decide_layout();
                output["response"]["layout"] = layout;
            } else if (phase == "move") {
                core::GameState state;
                state.player1_side = core::Side::Red;
                state.current_side =
                    (my_color == kBotzoneRedPlayer) ? core::Side::Red : core::Side::Blue;
                state.phase = core::Phase::Player1Thinking;

                int dice = latest_req.value("dice", kNoPiece);
                if (dice < kMinDice || dice > kMaxDice) {
                    throw std::runtime_error("Missing or invalid dice");
                }
                state.current_dice = dice;

                if (!latest_req.contains("board") || !latest_req["board"].is_array()) {
                    throw std::runtime_error("Missing board");
                }
                const json& board_json = latest_req["board"];

                for (std::size_t x = 0; x < core::Board::kSize; ++x) {
                    for (std::size_t y = 0; y < core::Board::kSize; ++y) {
                        int val = kNoPiece;

                        if (board_json.size() > x) {
                            if (board_json[x].is_array() && board_json[x].size() > y) {
                                val = board_json[x][y].get<int>();
                            }
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

                ai::Move move = ai::decide_move(state);
                output["response"]["x0"] = static_cast<int>(move.from.x);
                output["response"]["y0"] = static_cast<int>(move.from.y);
                output["response"]["x1"] = static_cast<int>(move.to.x);
                output["response"]["y1"] = static_cast<int>(move.to.y);
            }
        }

        std::cout << output.dump() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Bot error: " << e.what() << std::endl;
        std::cout << "{\"response\":{}}" << std::endl;
    }

    return 0;
}