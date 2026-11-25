#ifndef VALIDATION_H_
#define VALIDATION_H_

#include <string>
#include <vector>

#include "game_state.h"

namespace core {

/// @brief 一条局面状态问题，供存档/读档错误提示与测试断言使用。
struct StateIssue {
    std::string field;    ///< 出问题的字段名，例如 player1_side / phase / current_side
    std::string message;  ///< 面向用户的中文原因
};

/// @brief 校验 GameState 的字段值域与阶段状态机是否自洽。
///
/// 这个函数只做"状态本身是否可能来自一局正常对局"的判断，不修改 state，
/// 也不替代 rules::check_move：走法合法性仍由 rules 负责。
///
/// 主要约束：
/// - player1_side 必须是红/蓝，不能是 None；
/// - LayOut 允许空棋盘和未就绪，winner 必须为 None；
/// - ThrowingDice 的 current_side 必须是红/蓝，且不能带骰子；
/// - Player1Thinking 要求 current_side == player1_side，Player2Thinking 要求相反；
/// - 只有 Thinking 阶段允许 current_dice，且必须在 1~6；
/// - 非 GameOver 阶段 winner 必须为 None；GameOver 阶段 winner 必须与棋盘一致；
/// - 每方棋子数不超过 6，同方编号不重复；
/// - ready 只作为布阵过渡标记，不强制非 LayOut 阶段为 true。
///
/// @return 所有发现的问题；空 vector 表示状态自洽。
/// @note 该函数不抛异常，便于存档反序列化时一次性收集所有问题。
[[nodiscard]] std::vector<StateIssue> validate_state(const GameState& state);

/// @brief 校验局面；有任何问题时抛出 std::invalid_argument。
/// @throws std::invalid_argument what() 中包含字段名与中文原因，可直接展示
void require_valid_state(const GameState& state);

}  // namespace core

#endif
