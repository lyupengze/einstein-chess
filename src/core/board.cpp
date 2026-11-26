/// @file src/core/board.cpp
/// @brief 棋盘的构造、查询与布阵。

#include "board.h"

#include <array>
#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "types.h"

namespace core {

Board::Board() : cells_{} {
}

bool Board::is_in_bounds(const Pos& p) const noexcept {
    return p.x < kSize && p.y < kSize;
}

const Piece& Board::at(const Pos& p) const {
    if (!is_in_bounds(p)) {
        throw std::out_of_range("棋盘坐标越界: (" + std::to_string(p.x) + ", " +
                                std::to_string(p.y) + ")");
    }

    return cells_[p.x][p.y];
}

// 复用 const 版本的边界检查，避免两处逻辑漂移；对象本身非 const，转换安全。
Piece& Board::at(const Pos& p) {
    return const_cast<Piece&>(static_cast<const Board*>(this)->at(p));
}

// 查询按全盘扫描：棋盘只有 25 格，不值得维护额外索引。
// 空格子的 id 恒为 0，因此红/蓝方按 id 0 查找不会命中。
bool Board::has_piece(Side side, int id) const {
    for (std::size_t i = 0; i < kSize; ++i) {
        for (std::size_t j = 0; j < kSize; ++j) {
            if (at({i, j}).side == side && at({i, j}).id == id) {
                return true;
            }
        }
    }
    return false;
}

std::optional<Pos> Board::find_piece(Side side, int id) const {
    for (std::size_t i = 0; i < kSize; ++i) {
        for (std::size_t j = 0; j < kSize; ++j) {
            if (at({i, j}).side == side && at({i, j}).id == id) {
                return Pos{i, j};
            }
        }
    }
    return std::nullopt;
}

// 只统计 side 匹配的格子；由于允许吃己方棋子，结果可能小于 kChessCount。
int Board::count_pieces(Side side) const {
    int count = 0;
    for (std::size_t i = 0; i < kSize; ++i) {
        for (std::size_t j = 0; j < kSize; ++j) {
            if (at({i, j}).side == side) {
                ++count;
            }
        }
    }
    return count;
}

const std::array<Pos, Board::kChessCount>& Board::start_positions(Side side) {
    switch (side) {
        case Side::Red:
            return kRedStartPositions;
        case Side::Blue:
            return kBlueStartPositions;
        case Side::None:
            break;
    }
    throw std::invalid_argument("初始区只属于红方或蓝方");
}

std::optional<int> Board::start_index(Side side, Pos pos) noexcept {
    if (side != Side::Red && side != Side::Blue) {
        return std::nullopt;
    }

    const std::array<Pos, kChessCount>& positions =
        side == Side::Red ? kRedStartPositions : kBlueStartPositions;
    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (positions[i] == pos) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

void Board::setup_custom(Side side, const std::array<int, kChessCount>& layout) {
    // 只接受红/蓝：None 与任何未来新增的枚举值都无法映射到实际布阵区。
    if (side != Side::Red && side != Side::Blue) {
        throw std::invalid_argument("side 必须是红方或蓝方");
    }

    // layout 必须是 1~6 的排列：用 seen 数组同时检查越界（id <= 0 或 > 6）与重复。
    std::array<bool, kChessCount> seen{};
    for (int id : layout) {
        if (id <= 0 || id > kChessCount || seen[id - 1]) {
            throw std::invalid_argument("不合法的 layout");
        }
        seen[id - 1] = true;
    }

    // 先清空该方棋子再写入，重复调用是“替换”。清空放在校验之后，
    // 非法 layout 不会破坏原棋盘（强异常保证）。
    for (std::size_t x = 0; x < kSize; ++x) {
        for (std::size_t y = 0; y < kSize; ++y) {
            Piece& piece = at({x, y});
            if (piece.side == side) {
                piece = {Side::None, 0};
            }
        }
    }

    // 落位顺序由 start_positions 的坐标表定义，布阵界面与棋盘共用同一份表。
    const std::array<Pos, kChessCount>& start = start_positions(side);
    for (int i = 0; i < kChessCount; ++i) {
        at(start[i]) = {side, layout[i]};
    }
}

// 输出格式（供 CLI 阅读）：
//     0  1  2  3  4     <- 列号 y
//   0 .  R1 B2 .  .    <- 行号 x 在行首，格子固定 3 个字符宽
// 空格子为 " . "，棋子为 " R<编号>"/" B<编号>"（R=红方，B=蓝方）。
std::string Board::to_string() const {
    std::ostringstream oss;
    oss << ' ';
    for (std::size_t i = 0; i < kSize; ++i) {
        oss << "  " << i;
    }
    oss << '\n';

    for (std::size_t i = 0; i < kSize; ++i) {
        oss << i;
        for (std::size_t j = 0; j < kSize; ++j) {
            const Piece& cell = at({i, j});
            switch (cell.side) {
                case Side::None:
                    oss << " . ";
                    break;
                case Side::Red:
                    oss << " R" << cell.id;
                    break;
                case Side::Blue:
                    oss << " B" << cell.id;
                    break;
            }
        }
        oss << '\n';
    }

    return oss.str();
}

}  // namespace core
