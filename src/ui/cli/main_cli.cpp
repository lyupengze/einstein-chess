#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "ai/ai.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "storage/save_manager.h"

namespace {

void save(storage::SaveManager& save_manager, const core::GameState& state) {
    std::cout << "请输入存档名 (直接回车默认名为 autosave) : ";
    std::string save_name;
    std::getline(std::cin, save_name);
    if (save_name.empty()) {
        save_name = "autosave";
    }
    save_manager.save_game(save_name, state);
    std::cout << "存档成功！\n";
}

void game_loop(core::Game& game, storage::SaveManager& save_manager) {
    while (game.get_state().phase != core::Phase::GameOver) {
        std::cout << "【当前棋盘】\n";
        std::cout << game.get_state().board.to_string() << "\n";

        auto current_side = game.get_state().current_side;
        if (current_side == game.get_state().player1_side) {
            if (game.get_state().phase == core::Phase::ThrowingDice) {
                std::cout << "轮到你了. 按回车掷骰子...";
                std::string pause;
                std::getline(std::cin, pause);

                if (pause == "save") {
                    save(save_manager, game.get_state());
                    continue;
                } else if (pause == "exit") {
                    return;
                }

                game.roll_dice();
                std::cout << "你掷出了: " << *game.get_state().current_dice << "\n";
            }
            auto selectable = core::rules::get_selectable_ids(game.get_state());
            while (true) {
                std::cout << "可选棋子编号: ";
                for (int id : selectable) {
                    std::cout << id << " ";
                }
                std::cout << "\n请输入起点和终点 (如 0 2 1 2): ";

                std::string move_input;
                std::getline(std::cin, move_input);

                if (move_input == "save") {
                    save(save_manager, game.get_state());
                    continue;
                } else if (move_input == "exit") {
                    return;
                }

                std::istringstream iss_move(move_input);
                int x1, y1, x2, y2;
                if (iss_move >> x1 >> y1 >> x2 >> y2) {
                    try {
                        game.make_move(
                            {static_cast<std::size_t>(x1), static_cast<std::size_t>(y1)},
                            {static_cast<std::size_t>(x2), static_cast<std::size_t>(y2)});
                    } catch (const std::exception& e) {
                        std::cout << "移动失败: " << e.what() << "\n\n";
                        continue;
                    }
                } else {
                    std::cout << "命令不合法\n\n";
                    continue;
                }
                break;
            }
        } else {
            if (game.get_state().phase == core::Phase::ThrowingDice) {
                game.roll_dice();
            }

            ai::Move move = ai::decide_move(game.get_state());
            game.make_move(move.from, move.to);

            std::cout << "电脑掷出了: " << *game.get_state().current_dice << "，从 (" << move.from.x
                      << "," << move.from.y << ") 走到了 (" << move.to.x << "," << move.to.y
                      << ")\n";

            std::cout << "按回车继续...";
            std::string pause;
            std::getline(std::cin, pause);
        }
    }

    std::cout << "【游戏已结束】\n";
    std::cout << game.get_state().board.to_string() << "\n";
    auto winner = game.get_state().winner;
    std::cout << core::side_name(winner) << " 获胜！\n";
    std::cout << "按回车返回主菜单...";
    std::string pause;
    std::getline(std::cin, pause);
}

}  // namespace

int main() {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    std::cout << "=== 爱因斯坦棋 (CLI) ===\n";

    storage::SaveManager save_manager("einstein_save.db");

    while (true) {
        std::cout << "1. 开始游戏\n";
        std::cout << "2. 读档\n";
        std::cout << "3. 退出\n";
        std::cout << "请选择: ";

        std::string choice_str;
        std::getline(std::cin, choice_str);

        if (choice_str == "3") {
            std::cout << "感谢你的游玩\n";
            break;
        }

        core::Game game;
        if (choice_str == "2") {
            auto saves = save_manager.list_saves();
            if (saves.empty()) {
                std::cout << "没有找到任何存档！\n";
                continue;
            }

            std::cout << "【可用存档】\n";
            for (size_t i = 0; i < saves.size(); ++i) {
                std::cout << i + 1 << ". " << saves[i] << "\n";
            }
            std::cout << "请选择要读取的存档编号: ";

            std::string save_id_str;
            std::getline(std::cin, save_id_str);
            try {
                int choice = std::stoi(save_id_str);
                if (choice > 0 && choice <= static_cast<int>(saves.size())) {
                    auto state = save_manager.load_game(saves[choice - 1]);
                    if (state) {
                        game.load_game(*state);
                        std::cout
                            << "已开始游戏 (在你的回合输入 save 可以存档, 输入 exit 可以退出)\n\n";
                        game_loop(game, save_manager);
                    }
                    continue;
                }
            } catch (...) {
                std::cout << "无效的选择。\n";
            }
        }
        if (choice_str != "1") {
            std::cout << "命令不合法\n\n";
            continue;
        }

        std::cout << "已开始游戏 (在你的回合输入 save 可以存档, 输入 exit 可以退出)\n\n";
        std::cout << "【选色阶段】\n";

        core::Side player1_side{core::Side::None};
        core::Side player2_side{core::Side::None};
        while (true) {
            std::cout << "请选择你方颜色 (1. 红色, 2. 蓝色；红色先手) : ";

            std::string color_str;
            std::getline(std::cin, color_str);

            if (color_str == "1") {
                std::cout << "你选择了红方\n\n";
                game.prepare_new_game(core::Side::Red);
                player1_side = core::Side::Red;
                player2_side = core::Side::Blue;
                break;
            } else if (color_str == "2") {
                std::cout << "你选择了蓝方\n\n";
                game.prepare_new_game(core::Side::Blue);
                player1_side = core::Side::Blue;
                player2_side = core::Side::Red;
                break;
            } else {
                std::cout << "命令不合法\n\n";
            }
        }

        std::cout << "【布阵阶段】\n";
        while (true) {
            std::cout << "请输入 " << core::side_name(player1_side)
                      << " 1~6 号棋子的排列 (例如 1 2 3 4 5 6) : \n";

            std::string layout_input;
            std::getline(std::cin, layout_input);
            std::istringstream iss_layout(layout_input);
            std::array<int, 6> player_layout;
            bool parse_success = true;
            for (int i = 0; i < 6; ++i) {
                if (!(iss_layout >> player_layout[i])) {
                    parse_success = false;
                    break;
                }
            }

            if (!parse_success) {
                std::cout << "命令不合法\n\n";
                continue;
            }

            try {
                game.submit_layout(player1_side, player_layout);
                game.submit_layout(player2_side, ai::decide_layout());
            } catch (const std::exception& e) {
                std::cout << "布阵失败: " << e.what() << "\n\n";
                continue;
            }

            break;
        }

        std::cout << "【开始游戏】\n";
        game_loop(game, save_manager);
    }

    return 0;
}