// AI 模块：为对局提供布阵与走子决策。
//
// 模块对外暴露两代实现，调用方通过 Level 选择强度：
// - Level::kRandom（第一版）：在全部合法着法中均匀随机，只保证决策合法，
//   适合最低难度或作为对照基线；
// - Level::kSearch（第二版）：固定深度的 Expectiminimax，己方/对方为 MAX/MIN
//   节点，掷骰为 CHANCE 节点（1~6 等概率），叶节点用"竞速分布表"估算胜率。
//
// 所有单步接口的版本参数都带默认值 kDefaultLevel，因此不关心版本的调用方
// 继续写 ai::decide_move(state) 就是在用当前最新版；升级默认版本只需改
// kDefaultLevel 一处，调用方无需改动。
//
// 规则、合法着法枚举与走子结算全部复用 core，AI 不维护第二套规则；
// 两代实现只在"如何从合法着法中做选择"上不同。

#ifndef AI_H_
#define AI_H_

#include <array>
#include <string>

#include "core/game_state.h"
#include "core/types.h"

namespace ai {

/// @brief AI 强度档位；每个档位对应一代独立的决策实现。
/// @note 新增版本时：在此追加枚举值，并同步修改 ai.cpp 中的 level_name 与
///       decide_move 分发；kDefaultLevel 是否跟随新版本由维护者显式决定。
enum class Level {
    kRandom = 0,  ///< 第一版：布阵与走子都均匀随机，只保证决策合法。
    kSearch = 1,  ///< 第二版：Expectiminimax 搜索 + 竞速胜率评估。
};

/// @brief 未显式指定版本时使用的 AI，始终指向当前最新版。
/// @note 这是"默认用最新版"的唯一开关，升级默认 AI 时只改这一行。
inline constexpr Level kDefaultLevel = Level::kSearch;

/// @brief 全部可选档位，按从弱到强排列，供 UI 生成下拉框与命令行菜单。
/// @note 新增档位时除了追加 Level 枚举值，也要把新值补进本数组，
///       否则界面上会少一个选项（decide_move 本身仍能接受它）。
inline constexpr std::array<Level, 2> kAllLevels = {Level::kRandom, Level::kSearch};

/// @brief 判断枚举值是否是已定义的 AI 档位。
/// @param level 待检查的档位
/// @return 属于 kAllLevels 时返回 true；越界的枚举值返回 false
/// @note 存档与协议边界用它拒绝 `static_cast<Level>(99)` 这类值，
///       避免把非法档位写进数据库或交给 decide_move。
[[nodiscard]] constexpr bool is_valid_level(Level level) noexcept {
    for (const Level candidate : kAllLevels) {
        if (candidate == level) {
            return true;
        }
    }
    return false;
}

/// @brief AI 版本的展示名，供 CLI / GUI 输出或日志使用。
/// @param level 版本档位
/// @return 中文名称；传入越界的枚举值时返回"未知"
[[nodiscard]] std::string level_name(Level level);

/// @brief 走子搜索配置，仅搜索版（Level::kSearch）使用。
struct SearchConfig {
    /// 搜索深度，按"走子回合数"计：1 表示只看当前这一步，默认 4。
    int max_depth = 4;
};

/// 随机生成 1~6 的一个排列，作为某一方的布阵方案。
/// 返回值语义与 core::Board::setup_custom 的 layout 参数一致：
/// 下标 i 对应己方第 i 个初始格（红方自左上、蓝方自右下按固定顺序排列）。
/// @note 两个版本共用同一套随机布阵策略，因此本接口不接受 Level；
///       版本差异只体现在走子决策上。
[[nodiscard]] std::array<int, 6> decide_layout();

/// @brief 显式设置随机种子，便于测试/复现。
/// 随机源本身是线程局部的，因此该种子只影响调用线程；
/// 两代 AI 共用同一份随机源，切换版本不会改变已设置的种子。
void set_random_seed(unsigned int seed);

/// @brief 估算当前行棋方的胜率，范围 [0, 1]。
/// 通过单方竞速分布表合成，不设"材料价值"项；终局直接返回 0/1。
/// @note 这是搜索版的诊断接口，第一版随机 AI 没有局面评估模型，
///       因此本接口不接受 Level，始终按 kSearch 的评估口径计算。
/// @note 供诊断与测试使用；decide_move 的叶节点评估与此一致。
[[nodiscard]] double estimate_win_rate(const core::GameState& state);

/// @brief 为当前回合方选择一步走法。
/// @param state 当前对局状态，只读
/// @param level AI 版本，默认 kDefaultLevel（当前为最新版 kSearch）
/// @return 一步合法走法；具体策略由 level 决定
/// @throws std::invalid_argument phase 不是 Player1Thinking / Player2Thinking，
///         或 level 不是已定义的版本
/// @throws std::runtime_error 枚举不到任何合法着法
[[nodiscard]] core::Move decide_move(const core::GameState& state, Level level = kDefaultLevel);

/// @brief 使用搜索版 + 自定义搜索配置选择一步走法。
/// @param state 当前对局状态，只读
/// @param config 搜索深度等参数；max_depth <= 0 时按 1 处理
/// @return 一步合法走法
/// @throws std::invalid_argument phase 不是 Player1Thinking / Player2Thinking
/// @throws std::runtime_error 枚举不到任何合法着法
/// @note 本重载隐含 Level::kSearch；需要第一版随机策略时请显式传 Level。
[[nodiscard]] core::Move decide_move(const core::GameState& state, const SearchConfig& config);

}  // namespace ai

#endif
