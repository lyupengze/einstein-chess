#include <iostream>

#include "core/game_state.h"
#include "core/rules.h"

int main() {
    core::GameState state;
    state.board.setup_initial();
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 3;  // 强行指定骰子为 3

    std::cout << "=== 初始棋盘 ===\n" << state.board.to_string() << "\n";

    // 测试1：非法方向（用可选中的 R3，从 [0,2] 向上走到 [0,1]）
    std::cout << "--- 测试1：非法方向 ---\n";
    try {
        core::rules::apply_move(state, {0, 2}, {0, 1});  // 直接用 apply_move，如果错了它会抛
        std::cout << "错误！这步棋不应该合法\n";
    } catch (const std::exception& e) {
        std::cout << "成功拦截错误: " << e.what() << "\n\n";
    }

    // 测试2：合法移动（R3 从 [0,2] 向下走到 [1,2]）
    std::cout << "--- 测试2：合法移动 ---\n";
    try {
        core::rules::apply_move(state, {0, 2}, {1, 2});
        std::cout << "移动成功！\n";
        std::cout << state.board.to_string() << "\n";
    } catch (const std::exception& e) {
        std::cout << "不应该失败: " << e.what() << "\n";
    }

    return 0;
}