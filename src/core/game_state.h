#ifndef GAME_STATE_H_
#define GAME_STATE_H_

#include <optional>

#include "board.h"
#include "types.h"

namespace core {

/// @brief 对局阶段状态机。
///
/// 正常流转：
/// - LayOut：双方各提交一次布阵（Game::submit_layout），两边的 ready 都为真后自动进入下阶段；
/// - ThrowingDice：每回合的起点，等待当前方掷骰（Game::roll_dice）；
/// - Player1Thinking / Player2Thinking：掷骰后等待当前方走子（Game::make_move）；
/// - 走子结算后若未分胜负，换手并回到 ThrowingDice；分出胜负则进入 GameOver。
///
/// Player1/Player2 指"轮到哪一方"，与红蓝无关：Player1 就是 player1_side 那一方
/// （CLI/GUI 中即人类），Player2 是另一方；先手固定为红方，但那是由
/// Game::check_both_ready 决定的，不是枚举本身规定的。
enum class Phase { LayOut, ThrowingDice, Player1Thinking, Player2Thinking, GameOver };

/// @brief 一局对局的完整快照，也是存档模块序列化的单位。
/// @note 纯数据、无行为；所有规则判定在 core::rules 与 Game 中，改这里不需要改规则。
struct GameState {
    Board board;                      ///< 5x5 棋盘
    Side player1_side{Side::None};    ///< 发起方（CLI/GUI 中即人类玩家）的阵营
    Side current_side{Side::None};    ///< 当前轮到哪一方；LayOut 阶段为 None
    Phase phase{Phase::LayOut};       ///< 当前阶段，见上方状态机说明
    std::optional<int> current_dice;  ///< 本回合骰子点数 1~6；未掷骰或走子结算后为 nullopt
    Side winner{Side::None};          ///< 胜者；未分出胜负时为 None，只有 GameOver 阶段有意义
    bool player1_ready{false};        ///< 布阵阶段：player1_side 一方是否已提交布阵
    bool player2_ready{false};        ///< 布阵阶段：另一方是否已提交布阵
};

}  // namespace core

#endif