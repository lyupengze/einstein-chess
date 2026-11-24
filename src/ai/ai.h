// AI 模块：为对局提供布阵与走子决策。
// 走子使用固定深度的 Expectiminimax：己方/对方为 MAX/MIN 节点，
// 掷骰为 CHANCE 节点（1~6 等概率），叶节点用"竞速分布表"估算胜率。
// 规则、合法着法枚举与走子结算全部复用 core，AI 不维护第二套规则。
// 接口统一按"输入局面 → 输出决策"设计，调用方无需关心搜索细节。

#ifndef AI_H_
#define AI_H_

#include <array>

#include "core/game_state.h"
#include "core/types.h"

namespace ai {

/// @brief 走子搜索配置。
struct SearchConfig {
    /// 搜索深度，按"走子回合数"计：1 表示只看当前这一步，默认 4。
    int max_depth = 4;
};

/// 随机生成 1~6 的一个排列，作为某一方的布阵方案。
/// 返回值语义与 core::Board::setup_custom 的 layout 参数一致：
/// 下标 i 对应己方第 i 个初始格（红方自左上、蓝方自右下按固定顺序排列）。
[[nodiscard]] std::array<int, 6> decide_layout();

/// @brief 显式设置随机种子，便于测试/复现。
/// 随机源本身是线程局部的，因此该种子只影响调用线程。
void set_random_seed(unsigned int seed);

/// @brief 估算当前行棋方的胜率，范围 [0, 1]。
/// 通过单方竞速分布表合成，不设"材料价值"项；终局直接返回 0/1。
/// @note 供诊断与测试使用；decide_move 的叶节点评估与此一致。
[[nodiscard]] double estimate_win_rate(const core::GameState& state);

/// @brief 使用默认 SearchConfig（深度 4）为当前回合方选择一步走法。
/// @throws std::invalid_argument phase 不是 Player1Thinking / Player2Thinking
/// @throws std::runtime_error 枚举不到任何合法着法
[[nodiscard]] core::Move decide_move(const core::GameState& state);

/// @brief 使用指定配置为当前回合方选择一步走法。
/// @param config 搜索深度等参数；max_depth <= 0 时按 1 处理
/// @throws std::invalid_argument phase 不是 Player1Thinking / Player2Thinking
/// @throws std::runtime_error 枚举不到任何合法着法
[[nodiscard]] core::Move decide_move(const core::GameState& state, const SearchConfig& config);

}  // namespace ai

#endif
