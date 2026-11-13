#include <iostream>

#include "core/game.h"

int main() {
    core::Game game;
    game.prepare_new_game(core::Side::Red);

    std::cout << "=== 1. 游戏准备阶段 ===\n";
    auto state = game.get_state();
    std::cout << "当前阶段 (0=LayOut): " << static_cast<int>(state.phase) << "\n\n";

    std::cout << "=== 2. 红方布阵 ===\n";
    // 红方故意用乱序布阵：3, 1, 2, 5, 4, 6
    std::array<int, 6> red_layout = {3, 1, 2, 5, 4, 6};
    game.submit_layout(core::Side::Red, red_layout);

    state = game.get_state();
    std::cout << "红方已就绪: " << (state.player1_ready ? "是" : "否") << "\n";
    std::cout << "当前阶段 (应仍为 0): " << static_cast<int>(state.phase) << "\n";
    std::cout << "当前棋盘：\n" << state.board.to_string() << "\n";

    std::cout << "=== 3. 蓝方布阵 ===\n";
    // 蓝方也用乱序：6, 5, 4, 3, 2, 1
    std::array<int, 6> blue_layout = {6, 5, 4, 3, 2, 1};
    game.submit_layout(core::Side::Blue, blue_layout);

    state = game.get_state();
    std::cout << "蓝方已就绪: " << (state.player2_ready ? "是" : "否") << "\n";
    std::cout << "当前阶段 (1=ThrowingDice): " << static_cast<int>(state.phase) << "\n";
    std::cout << "当前棋盘：\n" << state.board.to_string() << "\n";

    return 0;
}