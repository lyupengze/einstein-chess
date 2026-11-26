/// @file src/core/board.h
/// @brief 5x5 棋盘的存放与查询。
///
/// 只负责数据，不做任何走法判定；规则全部在 core::rules 中。

#ifndef BOARD_H_
#define BOARD_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "types.h"

namespace core {

/// @brief 5x5 棋盘：按 [x][y] 存放棋子，行列下标即 Pos.x / Pos.y，不做转置。
class Board {
public:
    static constexpr int kChessCount{6};    ///< 每方棋子数，也是骰子点数上限
    static constexpr std::size_t kSize{5};  ///< 棋盘边长（5x5）

    /// @brief 构造空棋盘：25 格全部为 {Side::None, 0}。
    Board();

    /// @brief 坐标是否落在棋盘内。纯边界判断，不抛异常。
    [[nodiscard]] bool is_in_bounds(const Pos& p) const noexcept;

    /// @brief 读取指定格子（只读）。
    /// @throws std::out_of_range 坐标越界
    [[nodiscard]] const Piece& at(const Pos& p) const;

    /// @brief 读取指定格子（可写）。
    /// @throws std::out_of_range 坐标越界
    [[nodiscard]] Piece& at(const Pos& p);

    /// @brief 指定阵营的某个编号棋子是否还在场上。
    /// @note 全盘扫描 25 格；id 传 0 永远返回 false（空格编号恒为 0）。
    [[nodiscard]] bool has_piece(Side side, int id) const;

    /// @brief 定位指定阵营、指定编号棋子的坐标。
    /// @return 找到返回坐标，不在场上返回 std::nullopt
    [[nodiscard]] std::optional<Pos> find_piece(Side side, int id) const;

    /// @brief 统计某方剩余棋子数。允许吃己方棋子，所以结果可能小于 kChessCount。
    [[nodiscard]] int count_pieces(Side side) const;

    /// @brief 用给定排列在初始区布阵（布阵阶段调用）。
    ///
    /// layout[i] 依次对应该方第 i 个初始格，落位顺序必须与 SetupDialog::get_index_in_region 一致。
    /// 写入前先清空该方全部棋子，因此重复调用是“替换”而不是“叠加”。
    /// @param side 必须是 Side::Red 或 Side::Blue
    /// @param layout 必须是 1~6 的一个排列（不可缺号、不可重复）
    /// @throws std::invalid_argument side 非法，或 layout 不是合法排列
    /// @note 非法 layout 在清空之前就被拒绝，原棋盘保持不变。
    void setup_custom(Side side, const std::array<int, kChessCount>& layout);

    /// @brief 生成供 CLI 文本界面打印的棋盘快照。
    /// @return 首行列号、每行行号 + 格子内容；空格为 " . "，棋子为 " R1" / " B3"
    [[nodiscard]] std::string to_string() const;

private:
    /// 按 [x][y] 存放，x 为行、y 为列；空格用 {Side::None, 0} 表示。
    std::array<std::array<Piece, kSize>, kSize> cells_{};
};

}  // namespace core

#endif  // BOARD_H_