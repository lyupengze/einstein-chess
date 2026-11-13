#include "board.h"

#include <array>
#include <cstddef>
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
        throw std::out_of_range("p is illegal");
    }

    return cells_[p.x][p.y];
}

Piece& Board::at(const Pos& p) {
    return const_cast<Piece&>(static_cast<const Board*>(this)->at(p));
}

void Board::setup_initial() {
    cells_ = {};
    constexpr int kChessCount{6};
    constexpr std::array<Pos, kChessCount> kRedPos{
        {{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}}};
    constexpr std::array<Pos, kChessCount> kBluePos{
        {{4, 4}, {4, 3}, {4, 2}, {3, 4}, {3, 3}, {3, 2}}};

    for (int i = 0; i < kChessCount; ++i) {
        at(kRedPos[i]) = {Side::Red, i + 1};
        at(kBluePos[i]) = {Side::Blue, i + 1};
    }
}

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
            const Piece& cell = at({j, i});
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
                default:
                    break;
            }
        }
        oss << '\n';
    }

    return oss.str();
}

}  // namespace core