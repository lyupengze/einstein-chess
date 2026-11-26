/// @file src/core/game_state.h
/// @brief 对局阶段与局面快照。

#pragma once

#include <optional>

#include "board.h"
#include "types.h"

namespace core {

/// @brief 对局阶段。
///
/// 流转：LayOut（双方布阵）→ ThrowingDice（等待当前方掷骰）→ Player1Thinking / Player2Thinking
/// （等待走子）→ 未分胜负回到 ThrowingDice，分出胜负进入 GameOver。
///
/// Player1/Player2 指“轮到哪一方”，与红蓝无关：Player1 是 player1_side 那一方
/// （CLI/GUI 中即人类），Player2 是另一方。先手固定为红方，由 Game::check_both_ready 决定。
enum class Phase { LayOut, ThrowingDice, Player1Thinking, Player2Thinking, GameOver };

/// @brief 一局对局的完整快照，也是存档模块序列化的单位。
/// @note 纯数据、无行为；规则判定在 core::rules 与 Game 中，这里不复制规则。
struct GameState {
    Board board;                      ///< 5x5 棋盘
    Side player1_side{Side::None};    ///< 发起方（CLI/GUI 中即人类）的阵营
    Side current_side{Side::None};    ///< 当前轮到哪一方；布阵阶段为 None
    Phase phase{Phase::LayOut};       ///< 当前阶段
    std::optional<int> current_dice;  ///< 本回合骰子点数 1~6；未掷骰或结算后为 nullopt
    Side winner{Side::None};          ///< 胜者；未分胜负为 None，仅 GameOver 阶段有意义
    bool player1_ready{false};        ///< player1_side 一方是否已提交布阵
    bool player2_ready{false};        ///< 另一方是否已提交布阵
};

}  // namespace core
