/// @file src/ui/cli/main_cli.cpp
/// @brief 命令行对局：选档位、选色、布阵、掷骰与走子。
///
/// 对手由 ai 模块扮演，存档经 `storage::SaveManager` 写入 SQLite；人类回合内
/// 输入 save 存档、exit 返回主菜单。`--seed` 固定随机种子便于复现同一局，
/// `--db` 指定存档库。所有输入都按整行读取再自行解析，避免 `operator>>`
/// 在输入缓冲区留下换行残渣。

#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "ai/ai.h"
#include "cli_input.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/rules.h"
#include "core/types.h"
#include "storage/save_manager.h"

namespace {

// 读取一整行输入。stdin 到达 EOF（管道读完、Ctrl+D / Ctrl+Z）时返回 false：
// 调用方必须立刻退出，否则 getline 会持续失败、外层循环拿着空串空转成死循环。
bool read_line(std::string& line) {
    return static_cast<bool>(std::getline(std::cin, line));
}

// 询问存档名并立即保存，直接回车使用 autosave；level 是当局档位，一并写入存档。
// 返回 false 表示输入已结束，调用方应结束本局。
bool save(storage::SaveManager& save_manager, const core::GameState& state, ai::Level level) {
    std::cout << "请输入存档名 (直接回车默认名为 autosave) : ";
    std::string save_name;
    if (!read_line(save_name)) {
        return false;
    }
    if (save_name.empty()) {
        save_name = "autosave";
    }
    // 同名槽位会被覆盖（见 SaveManager::save_game）。写库失败不应该终止进程，
    // 打印原因后继续对局，由玩家决定是否重试。
    try {
        save_manager.save_game(save_name, state, level);
        std::cout << "存档成功！\n";
    } catch (const std::exception& e) {
        std::cout << "存档失败: " << e.what() << "\n";
    }
    return true;
}

// 让玩家选择档位，选项由 ai::kAllLevels 生成，不在此写死；直接回车取
// kDefaultLevel。返回 std::nullopt 表示输入已结束，调用方应立即退出。
std::optional<ai::Level> choose_ai_level() {
    std::cout << "【难度选择】\n";
    while (true) {
        std::cout << "请选择电脑强度:\n";
        for (std::size_t i = 0; i < ai::kAllLevels.size(); ++i) {
            std::cout << i + 1 << ". " << ai::level_name(ai::kAllLevels[i]) << "\n";
        }
        std::cout << "请输入编号 (直接回车默认 " << ai::level_name(ai::kDefaultLevel) << "): ";

        std::string level_input;
        if (!read_line(level_input)) {
            return std::nullopt;
        }
        if (level_input.empty()) {
            return ai::kDefaultLevel;
        }

        const auto index =
            cli::parse_level_index(level_input, static_cast<int>(ai::kAllLevels.size()));
        if (index) {
            return ai::kAllLevels[static_cast<std::size_t>(*index - 1)];
        }
        std::cout << "命令不合法\n\n";
    }
}

// 单局主循环，直到 phase 进入 GameOver 才返回。人类回合先掷骰再输入走法，
// 输错不消耗回合；AI 回合自动掷骰并走一步。ai_level 是当局档位，整局不变。
void game_loop(core::Game& game, storage::SaveManager& save_manager, ai::Level ai_level) {
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
                if (!read_line(pause)) {
                    std::cout << "\n输入已结束，本局结束。\n";
                    return;
                }

                if (pause == "save") {
                    if (!save(save_manager, game.get_state(), ai_level)) {
                        return;
                    }
                    // 回到循环开头重新提示掷骰。
                    continue;
                } else if (pause == "exit") {
                    // 放弃本局，回到主菜单。
                    return;
                }

                try {
                    game.roll_dice();
                } catch (const std::exception& e) {
                    // 正常流程不应走到这里；一旦阶段/行棋方异常，继续同一局只会
                    // 反复失败，因此直接中止本局回主菜单，让玩家重新开局或读档。
                    std::cout << "掷骰失败: " << e.what() << "\n";
                    std::cout << "本局已中止，返回主菜单。\n";
                    return;
                }
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
                if (!read_line(move_input)) {
                    std::cout << "\n输入已结束，本局结束。\n";
                    return;
                }

                // save / exit 在等待走子时同样可用。
                if (move_input == "save") {
                    if (!save(save_manager, game.get_state(), ai_level)) {
                        return;
                    }
                    continue;
                } else if (move_input == "exit") {
                    return;
                }

                // 先整行解析出 4 个整数；必须恰好 4 个，多余 token 也视为格式错误。
                std::array<int, 4> move_values{};
                if (cli::parse_int_array(move_input, move_values)) {
                    try {
                        game.make_move({static_cast<std::size_t>(move_values[0]),
                                        static_cast<std::size_t>(move_values[1])},
                                       {static_cast<std::size_t>(move_values[2]),
                                        static_cast<std::size_t>(move_values[3])});
                    } catch (const std::exception& e) {
                        std::cout << "移动失败: " << e.what() << "\n\n";
                        continue;
                    }
                } else {
                    std::cout << "命令不合法\n\n";
                    continue;
                }
                // 走子成功，结束输入循环。
                break;
            }
        } else {
            // AI 回合：掷骰、搜索、落子都在同一段异常边界内。任何一步失败都中止
            // 本局回主菜单，避免在可疑状态上继续操作；Game 的失败路径保证原状态
            // 不被部分修改。
            try {
                // 补上掷骰这一步，与人类回合保持相同的阶段流转。
                if (game.get_state().phase == core::Phase::ThrowingDice) {
                    game.roll_dice();
                }

                // make_move 结束时会把 current_dice 复位为空 optional，所以必须在走子
                // 之前先把点数存到局部变量；走子之后再解引用 *current_dice 是未定义行为
                // （Release 下只是恰好能从残留存储里读到旧点数，看起来“正常”）。
                const int ai_dice = game.get_state().current_dice.value_or(0);

                core::Move move = ai::decide_move(game.get_state(), ai_level);
                game.make_move(move.from, move.to);

                std::cout << "电脑掷出了: " << ai_dice << "，从 (" << move.from.x << ","
                          << move.from.y << ") 走到了 (" << move.to.x << "," << move.to.y << ")\n";
            } catch (const std::exception& e) {
                std::cout << "电脑回合失败: " << e.what() << "\n";
                std::cout << "本局已中止，返回主菜单。\n";
                return;
            }

            // 停一下，让玩家看清 AI 这一手再进入下一轮。
            std::cout << "按回车继续...";
            std::string pause;
            if (!read_line(pause)) {
                std::cout << "\n输入已结束，本局结束。\n";
                return;
            }
        }
    }

    // 对局结束：展示终局棋盘与获胜方，等回车后由调用方回主菜单。
    std::cout << "【游戏已结束】\n";
    std::cout << game.get_state().board.to_string() << "\n";
    auto winner = game.get_state().winner;
    std::cout << core::side_name(winner) << " 获胜！\n";
    std::cout << "按回车返回主菜单...";
    std::string pause;
    // 忽略返回值：EOF 时直接返回主菜单，不会卡住。
    read_line(pause);
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Windows 控制台的默认代码页通常不是 UTF-8，直接输出中文和棋盘符号会乱码。
    // 这里用 Win32 API 切换输入/输出代码页，而不是 system("chcp ...")：
    // 不启动 cmd.exe 子进程，也不会让 chcp 读走被重定向的 stdin 第一行。
    const bool output_ok = SetConsoleOutputCP(CP_UTF8) != 0;
    const bool input_ok = SetConsoleCP(CP_UTF8) != 0;
    if (!output_ok || !input_ok) {
        std::cerr << "警告: 无法将 Windows 控制台切换到 UTF-8，中文可能显示异常。\n";
    }
#endif
    std::cout << "=== 爱因斯坦棋 (CLI) ===\n";

    // --seed <n>：固定 AI 随机种子，同种子可复现同一局。
    // --db <path>：显式指定存档库；不指定时仍跟随当前工作目录。
    std::string db_path(storage::kDefaultDbPath);
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--seed") {
            if (i + 1 >= argc) {
                std::cerr << "--seed 需要一个 0.." << std::numeric_limits<unsigned int>::max()
                          << " 的整数\n";
                return 2;
            }
            const std::string_view seed_text = argv[++i];
            const auto seed = cli::parse_seed(seed_text);
            if (!seed) {
                std::cerr << "--seed 需要一个 0.." << std::numeric_limits<unsigned int>::max()
                          << " 的整数，收到: " << std::string(seed_text) << "\n";
                return 2;
            }
            ai::set_random_seed(*seed);
        } else if (arg == "--db") {
            if (i + 1 >= argc) {
                std::cerr << "--db 需要一个存档文件路径\n";
                return 2;
            }
            db_path = argv[++i];
            if (db_path.empty()) {
                std::cerr << "--db 的路径不能为空\n";
                return 2;
            }
        } else {
            std::cerr << "未知参数: " << arg << "（可用: --seed <n>, --db <路径>）\n";
            return 2;
        }
    }

    // 把即将使用的存档库绝对路径告诉玩家：CLI 默认跟随启动目录，
    // 不打印的话用户很难判断刚才的存档到底落在哪里。
    std::error_code path_error;
    const std::filesystem::path absolute_db = std::filesystem::absolute(db_path, path_error);
    const std::string db_display = path_error ? db_path : absolute_db.string();
    std::cout << "存档会保存在: " << db_display << "\n";

    // 打开存档库（文件不存在会自动创建）；打不开（工作目录不可写、
    // 路径被同名目录占用等）时打印指引并退出，而不是让异常穿出 main。
    std::unique_ptr<storage::SaveManager> save_manager;
    try {
        save_manager = std::make_unique<storage::SaveManager>(db_path);
    } catch (const std::exception& e) {
        std::cerr << "无法打开存档库 " << db_display << ": " << e.what() << "\n"
                  << "请确认存档位置可写；默认是当前工作目录，可用 --db 指定其他文件。\n";
        return 1;
    }

    // 主菜单循环：每轮处理一次选择。
    while (true) {
        std::cout << "1. 开始游戏\n";
        std::cout << "2. 读档\n";
        std::cout << "3. 退出\n";
        std::cout << "请选择: ";

        std::string choice_str;
        if (!read_line(choice_str)) {
            std::cout << "\n输入已结束，退出。\n";
            break;
        }

        if (choice_str == "3") {
            std::cout << "感谢你的游玩\n";
            break;
        }

        // 每轮新建一个 Game 对象承载当前对局：
        // 无论走"读档"还是"新局"，后续逻辑都只操作这个实例。
        core::Game game;
        if (choice_str == "2") {
            // 列存档只依赖数据库；表结构损坏、被锁或 IO 失败都不应终止进程，
            // 打印原因后回到主菜单，玩家仍可开新局或退出。
            std::vector<std::string> saves;
            try {
                saves = save_manager->list_saves();
            } catch (const std::exception& e) {
                std::cout << "读取存档列表失败: " << e.what() << "\n\n";
                continue;
            }
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
            if (!read_line(save_id_str)) {
                std::cout << "\n输入已结束，退出。\n";
                return 0;
            }
            try {
                int choice = std::stoi(save_id_str);
                if (choice > 0 && choice <= static_cast<int>(saves.size())) {
                    // 读档失败（存档被改坏、字段不自洽等）只影响这一次读取：
                    // 打印具体原因后回主菜单，不能把异常吞成一句"无效的选择"。
                    try {
                        auto saved = save_manager->load_game(saves[choice - 1]);
                        if (saved) {
                            game.load_game(saved->state);
                            // 档位来自存档本身：同一份档案每次读都恢复成同一个档位，
                            // 不受主菜单里临时选择的档位影响。
                            std::cout << "已开始游戏 (电脑强度: " << ai::level_name(saved->ai_level)
                                      << "; 在你的回合输入 save 可以存档, 输入 exit 可以退出)\n\n";
                            game_loop(game, *save_manager, saved->ai_level);
                        } else {
                            std::cout << "该存档槽位没有数据。\n";
                        }
                    } catch (const std::exception& e) {
                        std::cout << "读档失败: " << e.what() << "\n";
                    }
                    // 读档后（无论是否取到数据）都回主菜单，不落入新局流程。
                    continue;
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

        // 档位在选色前确定，之后整局（含存档）都使用它。
        const auto selected_level = choose_ai_level();
        if (!selected_level) {
            std::cout << "\n输入已结束，退出。\n";
            return 0;
        }
        const ai::Level ai_level = *selected_level;
        std::cout << "电脑强度: " << ai::level_name(ai_level) << "\n\n";

        std::cout << "【选色阶段】\n";

        // player1 = 人类，player2 = AI；这一角色由 prepare_new_game 的参数
        // 固定下来，与红蓝无关。红色先手由 check_both_ready 保证。
        core::Side player1_side{core::Side::None};
        core::Side player2_side{core::Side::None};
        while (true) {
            std::cout << "请选择你方颜色 (1. 红色, 2. 蓝色；红色先手) : ";

            std::string color_str;
            if (!read_line(color_str)) {
                std::cout << "\n输入已结束，退出。\n";
                return 0;
            }

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

            // 整行读入后按空白切出 6 个整数；必须恰好 6 个，
            // 不足或多出 token 都视为格式错误。
            std::string layout_input;
            if (!read_line(layout_input)) {
                std::cout << "\n输入已结束，退出。\n";
                return 0;
            }
            std::array<int, 6> player_layout{};
            if (!cli::parse_int_array(layout_input, player_layout)) {
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
        game_loop(game, *save_manager, ai_level);
    }

    return 0;
}