#ifndef BOARD_H_
#define BOARD_H_

#include <array>
#include <cstddef>
#include <string>

#include "types.h"

namespace core {

class Board {
public:
    Board();

    [[nodiscard]] bool is_in_bounds(const Pos& p) const noexcept;

    [[nodiscard]] const Piece& at(const Pos& p) const;
    [[nodiscard]] Piece& at(const Pos& p);

    void setup_initial();

    [[nodiscard]] std::string to_string() const;

private:
    static constexpr std::size_t kSize{5};
    std::array<std::array<Piece, kSize>, kSize> cells_{};
};

}  // namespace core

#endif