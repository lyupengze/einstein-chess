#ifndef TYPES_H_
#define TYPES_H_

#include <cstddef>
#include <string>

namespace core {

struct Pos {
    std::size_t x{};
    std::size_t y{};
};

inline bool operator==(const Pos& a, const Pos& b) {
    return a.x == b.x && a.y == b.y;
}

enum class Side { None, Red, Blue };

struct Piece {
    Side side{Side::None};
    int id{};
};

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