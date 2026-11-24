#ifndef RULES_H_
#define RULES_H_

#include <string>
#include <vector>

#include "game_state.h"
#include "types.h"

namespace core::rules {

/// 爱因斯坦棋规则（本项目实现口径；改规则需同步 GUI 状态文案与 README）：
/// 1. 每回合先掷骰，点数 N 对应 N 号棋子；N 号不在场时"降级选择"——
///    候选为编号小于 N 的最大者、大于 N 的最小者（各自最多一枚，可能同时有两个）；
/// 2. 红方每步只能走一格：向下 (x+1)、向右 (y+1)、向右下 (x+1,y+1)；
///    蓝方完全对称：向上、向左、向左上；
/// 3. 落点只要求仍在棋盘内，不要求为空——吃对方棋子与吃己方棋子都合法（规则如此，不是缺漏）；
/// 4. 胜负：一方棋子被吃光，或红方占据右下角 (4,4) / 蓝方占据左上角 (0,0)。

/// @brief 算出本回合"可以被选中移动"的棋子编号集合（降级选择规则见上）。
/// @param state 只读取 current_dice 与 current_side，不修改
/// @return 合法编号集合；current_dice 为空（还没掷骰）时返回空集合，
///         因此"没掷骰就想走子"会被 check_move 间接拦下
[[nodiscard]] std::vector<int> get_selectable_ids(const GameState& state);

/// @brief 校验一步走法是否合法。
/// @param state 当前对局状态，只读
/// @param from 起点；必须是当前方棋子且属于本回合可选编号
/// @param to 终点；只校验在棋盘内，**不校验占位**（允许吃己方棋子）
/// @return 合法返回空字符串；非法返回面向用户的错误原因（中文），可直接展示
/// @note 本函数不校验 phase：调用前应确保处于 Thinking 阶段（current_dice 已掷出）
[[nodiscard]] std::string check_move(const GameState& state, Pos from, Pos to);

/// @brief 枚举当前回合方的全部合法着法。
/// @param state 只读；读取 current_side、current_dice 与棋盘
/// @return 合法着法列表；未掷骰或当前方没有棋子时返回空
/// @note 不校验 phase；最终合法性仍由 check_move 裁决，不在这里复制规则。
[[nodiscard]] std::vector<Move> generate_legal_moves(const GameState& state);

/// @brief 执行一步已经校验过的走子：落点被覆盖（即吃子），起点清空。
/// @throws std::invalid_argument check_move 判定非法时抛出，what() 为中文原因
/// @note 只改棋盘；换手、判胜、清骰由 core::apply_move_and_settle 负责
///       （Game::make_move 只是委托它）。
void apply_move(GameState& state, Pos from, Pos to);

/// @brief 判定当前局面是否已分出胜负。
/// @return 胜方；未分胜负返回 Side::None
/// @note 三个条件按顺序判定：红方全灭 → 蓝胜；蓝方全灭 → 红胜；
///       红方在 (4,4) → 红胜；蓝方在 (0,0) → 蓝胜
[[nodiscard]] Side check_winner(const GameState& state);

}  // namespace core::rules

#endif