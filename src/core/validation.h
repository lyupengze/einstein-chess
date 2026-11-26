/// @file src/core/validation.h
/// @brief 局面状态的一致性校验。

#pragma once

#include <string>
#include <vector>

#include "game_state.h"

namespace core {

/// @brief 一条局面状态问题，供存档错误提示与测试断言使用。
struct StateIssue {
    std::string field;    ///< 出问题的字段名，例如 current_side
    std::string message;  ///< 面向用户的中文原因
};

/// @brief 校验 GameState 的字段值域与阶段状态机是否自洽。
///
/// 判断的是“这个状态是否可能来自一局正常对局”，不修改 state，也不判断走法合法性。
/// 主要约束：
/// - player1_side 必须是红/蓝；winner 只在 GameOver 阶段可为红/蓝；
/// - LayOut 没有 current_side，且不能双方都已就绪；
/// - ThrowingDice 必须有可实际行棋的 current_side；
/// - Player1Thinking 要求 current_side == player1_side，Player2Thinking 要求相反；
/// - 骰子只在思考阶段存在，且必须是 1~6；
/// - 每方棋子数不超过 6，同方编号不重复；
/// - GameOver 的 winner 必须与棋盘一致，非 GameOver 阶段棋盘不能已满足胜利条件。
///
/// @return 所有发现的问题；空 vector 表示状态自洽。
/// @note 不抛异常，便于反序列化时一次性收集所有问题。
[[nodiscard]] std::vector<StateIssue> validate_state(const GameState& state);

/// @brief 校验局面；有任何问题时抛 std::invalid_argument。
/// @throws std::invalid_argument what() 逐条列出字段名与中文原因，可直接展示
void require_valid_state(const GameState& state);

}  // namespace core
