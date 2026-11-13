#ifndef BOARD_H_
#define BOARD_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "types.h"

namespace core {

class Board {
public:
    static constexpr int kChessCount{6};
    static constexpr std::size_t kSize{5};

    Board();

    [[nodiscard]] bool is_in_bounds(const Pos& p) const noexcept;

    [[nodiscard]] const Piece& at(const Pos& p) const;
    [[nodiscard]] Piece& at(const Pos& p);

    [[nodiscard]] bool has_piece(Side side, int id) const;
    [[nodiscard]] std::optional<Pos> find_piece(Side side, int id) const;
    [[nodiscard]] int count_pieces(Side side) const;

    void setup_initial();

    [[nodiscard]] std::string to_string() const;

private:
    std::array<std::array<Piece, kSize>, kSize> cells_{};
};

}  // namespace core

#endif