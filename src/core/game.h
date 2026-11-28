/// @file src/core/game.h
/// @brief 对局门面：把阶段约束与规则判定组装成操作。

#pragma once

#include <array>
#include <cstdint>
#include <random>

#include "game_state.h"
#include "types.h"

namespace core {

/// @brief 一局骰子随机流的快照：种子与已经掷骰的次数。
/// @note 存档同时记录这两项，读档后丢弃前 rolls 个点数即可得到与存档时相同的下一掷，
///       读档续玩因此与存档前完全一致。
struct RngState {
    std::uint32_t seed{};   ///< 骰子随机源的种子
    std::uint64_t rolls{};  ///< 本局构造以来已经掷骰的次数
};

/// @brief 已掷次数的上限；超过它说明数据被手改过（正常对局远达不到）。
inline constexpr std::uint64_t kMaxRollCount = 1000000;

/// @brief 对局门面：持有 GameState 与随机数发生器。
///
/// CLI、GUI、Botzone 都只通过本类驱动对局，不直接改 GameState；阶段约束
/// （何时能掷骰、何时能走子）因此只有一处实现。违反调用时序抛 std::logic_error，
/// 参数非法抛 std::invalid_argument，由界面捕获并提示。
class Game {
public:
    /// @brief 构造一局默认状态：空盘、LayOut 阶段，种子取自 std::random_device。
    Game();

    /// @brief 用给定种子构造一局默认状态。
    /// @param seed 骰子随机源的种子；同一构建、同一平台上，同一种子的骰序相同
    /// @note 这里只固定骰子；AI 的随机源由 ai::set_random_seed 单独播种，两者必须
    ///       由调用方同时设置，否则“同种子复现”只覆盖一半
    explicit Game(std::uint32_t seed);

    /// @brief 重置为全新对局并记录发起方阵营。
    /// @param player1_side 发起方（CLI/GUI 中即人类）的阵营，必须是红方或蓝方
    /// @throws std::invalid_argument player1_side 不是红方或蓝方
    /// @note player1_side 只表示“哪一方是我方”，不代表先手；先手固定红方，
    ///       由 check_both_ready 在双方布阵完成后设置。
    void prepare_new_game(Side player1_side);

    /// @brief 提交某一方的布阵结果。
    /// @param layout 1~6 的排列，语义见 Board::setup_custom
    /// @throws std::logic_error 不在 LayOut 阶段、该方已提交过布阵
    /// @throws std::invalid_argument side 不是红蓝，或 layout 不是 1~6 的排列
    /// @note 双方都就绪后自动进入 ThrowingDice
    void submit_layout(Side side, const std::array<int, Board::kChessCount>& layout);

    /// @brief 从存档恢复整局状态。
    /// @param game_state 局面快照；拷贝进对局对象，调用方的局面不受影响
    /// @throws std::invalid_argument 状态不满足 core::validate_state 的约束
    /// @note 即使调用方来自 storage，这里也会重新校验，非法局面不会进入对局对象
    void load_game(const GameState& game_state);

    /// @brief 掷骰子并进入对应方的走子阶段。
    /// @throws std::logic_error 当前不在 ThrowingDice 阶段，或 current_side 无效
    /// @note 点数在 1~6 均匀分布；进入 Player1Thinking 还是 Player2Thinking 取决于
    ///       current_side 是否等于 player1_side，与红蓝无关
    void roll_dice();

    /// @brief 走一步棋，并完成判胜、换手、清骰等结算。
    /// @throws std::logic_error 不在思考阶段、没有骰子、当前方无效
    /// @throws std::invalid_argument 走法非法（原因来自 rules::check_move）
    void make_move(Pos from, Pos to);

    /// @brief 只读访问当前状态快照；引用生命周期与 Game 对象一致。
    [[nodiscard]] const GameState& get_state() const;

    /// @brief 本局骰子随机源使用的种子。
    /// @note 存档记录该值，读档端用“同一种子 + 已掷次数”还原骰序
    [[nodiscard]] std::uint32_t seed() const noexcept;

    /// @brief 本局自构造以来掷骰的次数。
    /// @note 读档端在还原种子后按该次数丢弃前若干点数，得到与存档时相同的下一掷
    [[nodiscard]] std::uint64_t roll_count() const noexcept;

    /// @brief 本局随机流的快照，可直接写进存档。
    [[nodiscard]] RngState rng_state() const noexcept;

    /// @brief 还原随机流：种子与已掷次数共同决定下一掷。
    /// @param rng 存档里记录的随机流快照
    /// @throws std::invalid_argument rng.rolls 超过 kMaxRollCount
    /// @note 只还原骰子；AI 的随机源由调用方用同一个种子重新播种
    void restore_rng_state(RngState rng);

private:
    /// @brief 双方布阵都完成后，进入掷骰阶段并指定红方先手。
    void check_both_ready();

    GameState game_state_;        ///< 当前对局快照
    std::mt19937 gen_;            ///< 骰子随机源；每局一个实例，避免跨局共享状态
    std::uint32_t seed_{};        ///< gen_ 的初始种子，存档与复现用
    std::uint64_t roll_count_{};  ///< 已掷次数，读档时用于推进到同一骰序位置
};

/// @brief 在 state 的副本上走子并完成结算，返回新状态。
/// @throws std::invalid_argument 走法非法（原因来自 rules::check_move）
/// @note 这是“走子 → 判胜 → 未分胜负则换手并回到 ThrowingDice → 清骰”的唯一实现；
///       Game::make_move 与 AI 搜索共用，避免两边结算顺序漂移。
[[nodiscard]] GameState apply_move_and_settle(const GameState& state, Pos from, Pos to);

}  // namespace core
