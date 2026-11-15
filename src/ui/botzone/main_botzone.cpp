#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "ai/ai.h"
#include "core/game_state.h"
#include "core/types.h"

using json = nlohmann::json;

int main() {
    std::string inputLine;
    if (!std::getline(std::cin, inputLine)) {
        return 0;
    }

    try {
        json input = json::parse(inputLine);

        int my_color = 0;
        if (input.contains("initdata") && input["initdata"].contains("player")) {
            my_color = input["initdata"]["player"].get<int>();
        }

        json output;
        output["response"] = json::object();

        const json& requests = input["requests"];

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
                state.current_side = (my_color == 0) ? core::Side::Red : core::Side::Blue;
                state.phase = core::Phase::Player1Thinking;

                state.current_dice = latest_req["dice"].get<int>();

                const json& board_json = latest_req["board"];
                for (std::size_t x = 0; x < 5; ++x) {
                    for (std::size_t y = 0; y < 5; ++y) {
                        int val = board_json[x][y].get<int>();
                        if (val > 0) {
                            state.board.at({x, y}) = {core::Side::Red, val};
                        } else if (val < 0) {
                            state.board.at({x, y}) = {core::Side::Blue, -val};
                        } else {
                            state.board.at({x, y}) = {core::Side::None, 0};
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