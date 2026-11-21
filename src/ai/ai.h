// AI 模块：为对局提供布阵与走子决策。
// 当前是"随机基线"占位实现：只保证产出的决策合法，不评估局面优劣，
// 目的是先让 CLI / GUI / Botzone 三端跑通完整对局，后续再替换为搜索算法。
// 接口统一按"输入局面 → 输出决策"设计，替换实现时调用方无需改动。

#ifndef AI_H_
#define AI_H_

#include <array>

#include "core/game_state.h"
#include "core/types.h"

namespace ai {

/// 一步走子：从 from 走到 to。
struct Move {
    core::Pos from;
    core::Pos to;
};

/// 随机生成 1~6 的一个排列，作为某一方的布阵方案。
/// 返回值语义与 core::Board::setup_custom 的 layout 参数一致：
/// 下标 i 对应己方第 i 个初始格（红方自左上、蓝方自右下按固定顺序排列）。
[[nodiscard]] std::array<int, 6> decide_layout();

/// @brief 显式设置随机种子，便于测试/复现。
/// 随机源本身是线程局部的，因此该种子只影响调用线程。
void set_random_seed(unsigned int seed);

/// 为当前回合方（state.current_side）随机选择一步合法走法。
/// @throws std::invalid_argument phase 不是 Player1Thinking / Player2Thinking
/// @throws std::runtime_error 枚举不到任何合法着法
[[nodiscard]] Move decide_move(const core::GameState& state);

}  // namespace ai

#endif