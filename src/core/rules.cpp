/// @file src/core/rules.cpp
/// @brief 选子、走法校验与胜负判定的实现。

#include "rules.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace core::rules {

namespace {

// 双方的三个合法方向；check_move 与 generate_legal_moves 共用，避免规则重复。
constexpr std::array<std::pair<int, int>, 3> kRedSteps{{{1, 0}, {0, 1}, {1, 1}}};
constexpr std::array<std::pair<int, int>, 3> kBlueSteps{{{-1, 0}, {0, -1}, {-1, -1}}};

const std::array<std::pair<int, int>, 3>* steps_for(Side side) noexcept {
    if (side == Side::Red) {
        return &kRedSteps;
    }
    if (side == Side::Blue) {
        return &kBlueSteps;
    }
    return nullptr;
}

// None 没有方向表：按“通过”处理，不额外编造方向错误。调用方在此之前
// 已因无棋子或无骰子被拒绝，这里只是兜底。
bool is_forward_step(Side side, int dx, int dy) noexcept {
    const auto* steps = steps_for(side);
    if (steps == nullptr) {
        return true;
    }
    for (const auto& step : *steps) {
        if (dx == step.first && dy == step.second) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<int> get_selectable_ids(const GameState& state) {
    // 还没掷骰就没有可动棋子；返回空集合即可，调用方无需特判。
    if (!state.current_dice) {
        return {};
    }
    const int dice = *state.current_dice;
    const Side side = state.current_side;

    const auto ids =
        selectable_ids_for(dice, [&](int id) { return state.board.has_piece(side, id); });
    std::vector<int> result;
    for (int id : ids) {
        if (id != -1) {
            result.push_back(id);
        }
    }
    return result;
}

std::string check_move(const GameState& state, Pos from, Pos to) {
    // 检查顺序刻意从"最基础的事实"到"最具体的规则"，保证错误文案尽量准确：
    // 越界 → 起点归属 → 是否可选 → 方向。
    if (!state.board.is_in_bounds(from) || !state.board.is_in_bounds(to)) {
        return "坐标超出棋盘范围";
    }

    const Piece& piece = state.board.at(from);
    if (piece.side != state.current_side) {
        return "起点不是己方棋子";
    }

    auto selectable = get_selectable_ids(state);
    bool is_selected = false;
    for (int id : selectable) {
        if (id == piece.id) {
            is_selected = true;
            break;
        }
    }
    if (!is_selected) {
        return "该棋子不可被移动";
    }

    int dx = static_cast<int>(to.x) - static_cast<int>(from.x);
    int dy = static_cast<int>(to.y) - static_cast<int>(from.y);

    // 每步只能走一格，且方向必须朝向对方半场；dx/dy 同时为 0 也会被这里拒绝。
    if (!is_forward_step(state.current_side, dx, dy)) {
        if (state.current_side == Side::Red) {
            return "红方只能向下、向右、向右下移动";
        }
        return "蓝方只能向上、向左、向左上移动";
    }

    // 到此为止没有检查 to 上有没有棋子：落点被占据是合法的，吃己方棋子同样合法。
    return "";
}

std::vector<Move> generate_legal_moves(const GameState& state) {
    std::vector<Move> moves;
    const auto* steps = steps_for(state.current_side);
    if (steps == nullptr || !state.current_dice) {
        return moves;
    }

    // 每个可选棋子最多尝试三个方向，候选仍交给 check_move 裁决。
    for (int id : get_selectable_ids(state)) {
        const auto from = state.board.find_piece(state.current_side, id);
        if (!from) {
            continue;
        }
        for (const auto& step : *steps) {
            const int nx = static_cast<int>(from->x) + step.first;
            const int ny = static_cast<int>(from->y) + step.second;
            const Pos to{static_cast<std::size_t>(nx), static_cast<std::size_t>(ny)};
            if (!state.board.is_in_bounds(to)) {
                continue;
            }
            if (check_move(state, *from, to).empty()) {
                moves.push_back({*from, to});
            }
        }
    }
    return moves;
}

void apply_move(GameState& state, Pos from, Pos to) {
    // 再次校验而不是信任调用方：apply_move 是 public 接口，越界访问会直接崩。
    std::string err = check_move(state, from, to);
    if (!err.empty()) {
        throw std::invalid_argument(err);
    }

    state.board.at(to) = state.board.at(from);
    state.board.at(from) = {Side::None, 0};
}

Side check_winner(const GameState& state) {
    // 先判"全灭"：允许吃己方的子，因此一方可能主动把自己吃光而判负。
    if (state.board.count_pieces(Side::Red) == 0) {
        return Side::Blue;
    }
    if (state.board.count_pieces(Side::Blue) == 0) {
        return Side::Red;
    }

    // 再判"抵达对方角"：红方目标 (4,4)，蓝方目标 (0,0)。
    if (state.board.at({Board::kSize - 1, Board::kSize - 1}).side == Side::Red) {
        return Side::Red;
    }
    if (state.board.at({0, 0}).side == Side::Blue) {
        return Side::Blue;
    }

    return Side::None;
}

}  // namespace core::rules