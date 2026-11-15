// 命令行版爱因斯坦棋：在单进程内完成"选色 → 布阵 → 掷骰 / 走子"的完整对局，
// 对手由 ai 模块扮演，存档通过 storage::SaveManager 写入 SQLite。
// 交互约定：人类回合内输入 save 可存档、输入 exit 返回主菜单。
// 所有输入都按整行读取再自行解析，避免 operator>> 在输入缓冲区留下换行残渣。

#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "ai/ai.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "storage/save_manager.h"

namespace {

// 询问存档名并立即保存；直接回车则使用默认名 autosave。
void save(storage::SaveManager& save_manager, const core::GameState& state) {
    std::cout << "请输入存档名 (直接回车默认名为 autosave) : ";
    std::string save_name;
    std::getline(std::cin, save_name);
    if (save_name.empty()) {
        save_name = "autosave";
    }
    // 同名槽位会被覆盖（见 SaveManager::save_game）。
    save_manager.save_game(save_name, state);
    std::cout << "存档成功！\n";
}

// 单局主循环，直到 phase 进入 GameOver 才返回。
// 人类回合：先掷骰子，再输入"起点行 起点列 终点行 终点列"；输错不消耗回合。
// AI 回合：自动掷骰并随机走一步，等待玩家按回车继续。
void game_loop(core::Game& game, storage::SaveManager& save_manager) {
    while (game.get_state().phase != core::Phase::GameOver) {
        // 每轮开头重绘棋盘：玩家能直接看到上一手（或读档）后的局面。
        std::cout << "【当前棋盘】\n";
        std::cout << game.get_state().board.to_string() << "\n";

        // 用 player1_side 区分"人类 / AI"而不是红蓝：
        // CLI 里玩家固定是 player1，选蓝方时 player1_side 就是 Blue。
        auto current_side = game.get_state().current_side;
        if (current_side == game.get_state().player1_side) {
            if (game.get_state().phase == core::Phase::ThrowingDice) {
                // 人类回合开始：先等一次回车再掷骰；这里同时兼作 save / exit 入口。
                std::cout << "轮到你了. 按回车掷骰子...";
                std::string pause;
                std::getline(std::cin, pause);

                if (pause == "save") {
                    save(save_manager, game.get_state());
                    continue;  // 回到循环开头重新提示掷骰
                } else if (pause == "exit") {
                    return;  // 放弃本局，回到主菜单
                }

                game.roll_dice();
                // roll_dice 成功返回后 current_dice 必定有值，可以安全解引用。
                std::cout << "你掷出了: " << *game.get_state().current_dice << "\n";
            }
            // 输入走法。合法性（边界、方向、归属、骰子限制）全部交给
            // Game::make_move → rules，失败只打印原因，不改状态也不换手。
            auto selectable = core::rules::get_selectable_ids(game.get_state());
            while (true) {
                std::cout << "可选棋子编号: ";
                for (int id : selectable) {
                    std::cout << id << " ";
                }
                std::cout << "\n请输入起点和终点 (如 0 2 1 2): ";

                std::string move_input;
                std::getline(std::cin, move_input);

                // save / exit 在等待走子时同样可用。
                if (move_input == "save") {
                    save(save_manager, game.get_state());
                    continue;
                } else if (move_input == "exit") {
                    return;
                }

                // 先整行解析出 4 个整数，格式不对就直接重输。
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
                break;  // 走子成功，结束输入循环
            }
        } else {
            // AI 回合：补上掷骰这一步，与人类回合保持相同的阶段流转。
            if (game.get_state().phase == core::Phase::ThrowingDice) {
                game.roll_dice();
            }

            ai::Move move = ai::decide_move(game.get_state());
            game.make_move(move.from, move.to);

            // 注意：make_move 结束时已把 current_dice 复位为空 optional，
            // 下面解引用 *current_dice 属于未定义行为——Release 下只是"碰巧"
            // 还能从残留存储读到旧点数。若要修，应在 make_move 之前先把点数
            // 保存到局部变量。
            std::cout << "电脑掷出了: " << *game.get_state().current_dice << "，从 (" << move.from.x
                      << "," << move.from.y << ") 走到了 (" << move.to.x << "," << move.to.y
                      << ")\n";

            // 停一下，让玩家看清 AI 这一手再进入下一轮。
            std::cout << "按回车继续...";
            std::string pause;
            std::getline(std::cin, pause);
        }
    }

    // 对局结束：展示终局棋盘与获胜方，等回车后由调用方回主菜单。
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
    // Windows 控制台切到 UTF-8 代码页，否则中文与棋盘符号会乱码。
    system("chcp 65001 > nul");
#endif
    std::cout << "=== 爱因斯坦棋 (CLI) ===\n";

    // 打开默认存档库（文件不存在会自动创建）；实际落盘位置由进程工作目录决定。
    storage::SaveManager save_manager(storage::kDefaultDbPath);

    // 主菜单循环：每轮处理一次选择。
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

        // 每轮新建一个 Game 对象承载当前对局：
        // 无论走"读档"还是"新局"，后续逻辑都只操作这个实例。
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

            // 展示编号从 1 开始、数组下标从 0 开始，读入后统一减 1。
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
                    continue;  // 读档后（无论是否取到数据）都回主菜单，不落入新局流程
                }
            } catch (...) {
                // std::stoi 失败（非数字、溢出）统一按无效选择处理。
                std::cout << "无效的选择。\n";
            }
        }
        // 走到这里说明不是有效的读档请求：只有 "1" 视为开新局，
        // 其余输入（包括超出范围的存档编号）都按非法命令处理。
        if (choice_str != "1") {
            std::cout << "命令不合法\n\n";
            continue;
        }

        std::cout << "已开始游戏 (在你的回合输入 save 可以存档, 输入 exit 可以退出)\n\n";
        std::cout << "【选色阶段】\n";

        // player1 = 人类，player2 = AI；这一角色由 prepare_new_game 的参数
        // 固定下来，与红蓝无关。红色先手由 check_both_ready 保证。
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

        // 布阵：人类输入 1~6 的排列，AI 立即随机布阵；双方就绪后
        // check_both_ready 自动进入掷骰阶段，固定红方先手。
        std::cout << "【布阵阶段】\n";
        while (true) {
            std::cout << "请输入 " << core::side_name(player1_side)
                      << " 1~6 号棋子的排列 (例如 1 2 3 4 5 6) : \n";

            // 整行读入后按空白切出 6 个整数；不足 6 个视为格式错误，
            // 多余的部分忽略。
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
                // 排列是否合法（1~6 各一个）由 Board::setup_custom 校验；
                // 抛异常时 Game 状态保持不变，可以放心重输。
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