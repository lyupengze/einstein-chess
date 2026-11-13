#include <iostream>

#include "core/game.h"
#include "core/rules.h"

int main() {
    core::Game game;
    game.new_game(core::Side::Red);

    std::cout << "=== 1. 新游戏开始 ===\n";
    std::cout << "当前行动方: " << core::side_name(game.get_state().current_side) << "\n";
    std::cout << "当前阶段 (0=ThrowingDice): " << static_cast<int>(game.get_state().phase)
              << "\n\n";

    std::cout << "=== 2. 掷骰子 ===\n";
    game.roll_dice();
    std::cout << "掷出点数: " << *game.get_state().current_dice << "\n";
    std::cout << "当前阶段 (1或2=Thinking): " << static_cast<int>(game.get_state().phase) << "\n";

    // 获取当前可选的棋子
    auto ids = core::rules::get_selectable_ids(game.get_state());
    std::cout << "可选棋子: ";
    for (int id : ids) {
        std::cout << id << " ";
    }
    std::cout << "\n\n";

    std::cout << "=== 3. 测试非法走棋 (向上走) ===\n";
    // 随便找个红方棋子，强行让它往上走(不合规)
    try {
        game.make_move({0, 2}, {0, 1});
        std::cout << "【失败】非法走棋没有被拦截！\n";
    } catch (const std::exception& e) {
        std::cout << "【成功】异常捕获: " << e.what() << "\n";
        // 此时检查 phase 是否还在 Thinking，没有被改乱
        std::cout << "抛出异常后，当前阶段 (应仍为Thinking): "
                  << static_cast<int>(game.get_state().phase) << "\n";
    }

    // 注意：为了后面的合法测试不报错，我们得找一个绝对合法的走法。
    // 最简单的，直接挑 ids[0] 这个棋子，从它所在的位置往下/右走。
    // 如果你不想写复杂的自动找位置，可以直接在这里结束测试。

    std::cout << "\n=== 4. 测试合法走棋 ===\n";
    // 假设我们选了 3 号棋子，它初始在 [0,2]。
    // 如果骰子是3，且我们往下走到 [1,2]
    try {
        game.make_move({0, 2}, {1, 2});
        std::cout << "【成功】走棋完毕！\n";
        std::cout << "走棋后的行动方: " << core::side_name(game.get_state().current_side)
                  << " (应该变成蓝方)\n";
        std::cout << "走棋后的阶段 (0=ThrowingDice): " << static_cast<int>(game.get_state().phase)
                  << "\n";
        std::cout << "走棋后的骰子 (应该被清空): "
                  << (game.get_state().current_dice ? "未清空" : "已清空") << "\n";
    } catch (const std::exception& e) {
        std::cout << "走棋失败: " << e.what() << "\n";
    }

    return 0;
}