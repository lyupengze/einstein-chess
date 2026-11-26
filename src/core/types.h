/// @file src/core/types.h
/// @brief 坐标、走子与阵营的基础类型。
///
/// 坐标约定：x 是行号（向下增大），y 是列号（向右增大），原点 (0,0) 在左上角。
/// 绘制按矩阵习惯处理：x（行）落到屏幕纵向、y（列）落到屏幕横向，不做转置。

#ifndef TYPES_H_
#define TYPES_H_

#include <cstddef>
#include <string>

namespace core {

/// @brief 棋盘坐标。
/// @warning 改动 x/y 的语义必须同步修改 BoardWidget 的坐标换算与 SetupDialog 的区域判断，
///          否则点击位置与棋子落点会错位。
struct Pos {
    std::size_t x{};  ///< 行号，合法范围 [0, Board::kSize)
    std::size_t y{};  ///< 列号，合法范围 [0, Board::kSize)
};

/// @brief 坐标相等比较。Pos 不参与排序，只提供 == / != 供容器查找与 optional 判断。
inline bool operator==(const Pos& a, const Pos& b) {
    return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const Pos& a, const Pos& b) {
    return !(a == b);
}

/// @brief 一步走子：从 from 走到 to。
/// @note 只描述“从哪到哪”，不保证合法；合法性由 rules::check_move 判定。
struct Move {
    Pos from;
    Pos to;
};

inline bool operator==(const Move& a, const Move& b) {
    return a.from == b.from && a.to == b.to;
}

inline bool operator!=(const Move& a, const Move& b) {
    return !(a == b);
}

/// @brief 棋子阵营。
/// @note None 同时承担“空格”与“未指定”两种含义：棋盘上用 {Side::None, 0} 表示空格，
///       GameState 尚未开始时也用它表示“还没定”，代码里不区分这两者。
enum class Side { None, Red, Blue };

/// @brief 棋盘上单个格子的内容。
/// @note 空格统一写成 {Side::None, 0}；读 id 之前必须先判断 side，否则会把 0 当成编号。
struct Piece {
    Side side{Side::None};
    int id{};  ///< 棋子编号 1~6；side 为 None 时无意义
};

/// @brief 阵营的中文名，供 CLI 文本输出与 GUI 状态栏显示。
/// @return 无 / 红方 / 蓝方 / 未知；返回值已带“方”字，拼接文案时不要再补。
///         default 分支兜住未来新增的阵营值，避免界面显示空串。
inline std::string side_name(Side s) {
    switch (s) {
        case Side::None:
            return "无";
        case Side::Red:
            return "红方";
        case Side::Blue:
            return "蓝方";
        default:
            return "未知";
    }
}

}  // namespace core

#endif
