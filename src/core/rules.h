/// @file src/core/rules.h
/// @brief 走法生成与胜负判定。
///
/// 规则的完整说明见 docs/RULES.md。这里只保留三条调用方容易误解的口径：
/// 骰点不在场时按“小于点数的最大编号、大于点数的最小编号”降级选子；
/// 落点只要在棋盘内即可，吃己方棋子合法；胜负按全灭或占角判定。

#pragma once

#include <array>
#include <string>
#include <vector>

#include "game_state.h"
#include "types.h"

namespace core::rules {

/// @brief 按降级选择规则，从“编号是否存活”中选出本回合可动的编号。
/// @param dice 骰子点数 1~6；越界时返回 {-1, -1}
/// @param is_alive 接收编号 1~6，返回该编号是否还在场
/// @return 最多两个编号，-1 表示空位；同号在场时只返回该编号。
///         顺序与 get_selectable_ids 一致：先“小于点数的最大编号”，再“大于点数的最小编号”。
/// @note 这是选子规则的单一口径：get_selectable_ids 与 AI 的竞速分布表都调用它。
template <typename IsAlive>
[[nodiscard]] std::array<int, 2> selectable_ids_for(int dice, IsAlive&& is_alive) noexcept {
    std::array<int, 2> result{-1, -1};
    if (dice < 1 || dice > 6) {
        return result;
    }
    if (is_alive(dice)) {
        result[0] = dice;
        return result;
    }
    for (int i = dice - 1; i >= 1; --i) {
        if (is_alive(i)) {
            result[0] = i;
            break;
        }
    }
    for (int i = dice + 1; i <= 6; ++i) {
        if (is_alive(i)) {
            result[1] = i;
            break;
        }
    }
    return result;
}

/// @brief 算出本回合可被选中移动的棋子编号集合。
/// @param state 只读取 current_dice 与 current_side，不修改
/// @return 合法编号集合；current_dice 为空（还没掷骰）时返回空集合
[[nodiscard]] std::vector<int> get_selectable_ids(const GameState& state);

/// @brief 校验一步走法是否合法。
/// @param state 当前对局状态，只读
/// @param from 起点；必须是当前方棋子且属于本回合可选编号
/// @param to 终点；只校验在棋盘内，不校验占位（允许吃己方棋子）
/// @return 合法返回空字符串；非法返回面向用户的中文原因，可直接展示
/// @note 不校验 phase：调用前应确保处于思考阶段（current_dice 已掷出）
[[nodiscard]] std::string check_move(const GameState& state, Pos from, Pos to);

/// @brief 枚举当前回合方的全部合法着法。
/// @param state 只读
/// @return 合法着法列表；未掷骰或当前方没有棋子时返回空
/// @note 不校验 phase；每个候选仍由 check_move 裁决，不在这里复制规则
[[nodiscard]] std::vector<Move> generate_legal_moves(const GameState& state);

/// @brief 执行一步已经校验过的走子：落点被覆盖（即吃子），起点清空。
/// @throws std::invalid_argument check_move 判定非法时抛出，what() 为中文原因
/// @note 只改棋盘；换手、判胜、清骰由 core::apply_move_and_settle 负责。
void apply_move(GameState& state, Pos from, Pos to);

/// @brief 判定当前局面是否已分出胜负。
/// @return 胜方；未分胜负返回 Side::None
/// @note 先判全灭，再判占角；两个条件不会指向不同胜者，判定顺序不影响结果。
[[nodiscard]] Side check_winner(const GameState& state);

}  // namespace core::rules
