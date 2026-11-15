#ifndef GAME_H_
#define GAME_H_

#include <array>
#include <random>

#include "game_state.h"
#include "types.h"

namespace core {

/// @brief 对局门面：持有 GameState 与随机数发生器，把"阶段约束 + 规则判定"组装成操作。
///
/// CLI、GUI、Botzone 三个入口都只通过本类驱动对局，不直接改 GameState；
/// 这样阶段约束（何时能掷骰、何时能走子）只有一处实现。
/// 非法操作一律抛 std::invalid_argument，界面负责捕获并提示。
class Game {
public:
    /// @brief 构造一局默认状态：空盘、LayOut 阶段，随机数种子取自 std::random_device。
    Game();

    /// @brief 重置为全新对局并记录发起方阵营。
    /// @param player1_side 发起方（CLI/GUI 中即人类）的阵营，不能为 None
    /// @note player1_side 只表示"哪一方是我方"，不代表先手；先手固定是红方，
    ///       由 check_both_ready 在双方布阵完成后设置。
    void prepare_new_game(Side player1_side);

    /// @brief 提交某一方的布阵结果。
    /// @param layout 1~6 的排列，语义见 Board::setup_custom
    /// @throws std::invalid_argument layout 非法或 side 为 None
    /// @note 同侧重复提交会覆盖原布阵；双方都就绪后自动进入 ThrowingDice
    void submit_layout(Side side, const std::array<int, 6>& layout);

    /// @brief 从存档恢复整局状态（直接替换，不做校验）。
    /// @note 数据合法性由 storage 反序列化阶段保证，这里信任入参
    void load_game(GameState game_state);

    /// @brief 掷骰子并进入对应方的走子阶段。
    /// @throws std::invalid_argument 当前不在 ThrowingDice 阶段
    /// @note 点数在 1~6 均匀分布；结果是 Player1Thinking 还是 Player2Thinking
    ///       取决于 current_side 是否等于 player1_side，与红蓝无关
    void roll_dice();

    /// @brief 走一步棋，并完成判胜、换手、清骰等结算。
    /// @throws std::invalid_argument 走法非法（原因来自 rules::check_move）
    /// @note 结算顺序：落子 → 判胜 → 未分胜负则换手并回到 ThrowingDice → 清空骰子；
    ///       分胜负则直接进入 GameOver。顺序有依赖，不要随意调整
    void make_move(Pos from, Pos to);

    /// @brief 只读访问当前状态快照；引用生命周期与 Game 对象一致。
    [[nodiscard]] const GameState& get_state() const;

private:
    /// @brief 双方布阵都完成后，进入掷骰阶段并指定红方先手。
    void check_both_ready();

    GameState game_state_;  ///< 当前对局快照
    std::mt19937 gen_;      ///< 骰子随机源；每局一个实例，避免跨局共享状态
};

}  // namespace core

#endif