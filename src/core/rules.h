#ifndef RULES_H_
#define RULES_H_

#include <string>
#include <vector>

#include "game_state.h"

namespace core::rules {

// 1. 获取当前回合可移动的棋子编号
// 传入状态，返回一个 vector，里面存放可以移动的棋子 ID（1~6）
// 注意：这个函数依赖于 state.current_side 和 state.current_dice 必须有值
[[nodiscard]] std::vector<int> get_selectable_ids(const GameState& state);

// 2. 检查移动是否合法
// 返回空字符串 "" 表示合法；否则返回不合法的原因（比如 "方向错误"）
// 这个函数会检查：起点是不是己方棋子、是否在可选范围内、方向是否正确
[[nodiscard]] std::string check_move(const GameState& state, Pos from, Pos to);

// 3. 执行移动（包含吃子）
// 把 from 位置的棋子移动到 to，并覆盖掉 to 原有的棋子
void apply_move(GameState& state, Pos from, Pos to);

// 4. 检查胜负
// 在每次移动后调用。传入最后一次移动的阵营。
// 如果该阵营满足了胜利条件，返回该阵营；否则返回 Side::None
[[nodiscard]] Side check_winner(const GameState& state);

}  // namespace core::rules

#endif  // RULES_H_