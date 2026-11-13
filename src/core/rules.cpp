#include "rules.h"

#include <stdexcept>
#include <vector>

namespace core::rules {

std::vector<int> get_selectable_ids(const GameState& state) {
    if (!state.current_dice) {
        return {};
    }
    int dice = *state.current_dice;
    Side side = state.current_side;

    if (state.board.has_piece(side, dice)) {
        return {dice};
    }

    std::vector<int> result;
    for (int i = dice - 1; i >= 1; --i) {
        if (state.board.has_piece(side, i)) {
            result.push_back(i);
            break;
        }
    }

    for (int i = dice + 1; i <= 6; ++i) {
        if (state.board.has_piece(side, i)) {
            result.push_back(i);
            break;
        }
    }
    return result;
}

std::string check_move(const GameState& state, Pos from, Pos to) {
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

    if (state.current_side == Side::Red) {
        if (!((dx == 1 && dy == 0) || (dx == 0 && dy == 1) || (dx == 1 && dy == 1))) {
            return "红方只能向下、向右、向右下移动";
        }
    } else if (state.current_side == Side::Blue) {
        if (!((dx == -1 && dy == 0) || (dx == 0 && dy == -1) || (dx == -1 && dy == -1))) {
            return "蓝方只能向上、向左、向左上移动";
        }
    }

    return "";
}

void apply_move(GameState& state, Pos from, Pos to) {
    std::string err = check_move(state, from, to);
    if (!err.empty()) {
        throw std::invalid_argument(err);
    }

    state.board.at(to) = state.board.at(from);
    state.board.at(from) = {Side::None, 0};
}

Side check_winner(const GameState& state) {
    if (state.board.count_pieces(Side::Red) == 0) {
        return Side::Blue;
    }
    if (state.board.count_pieces(Side::Blue) == 0) {
        return Side::Red;
    }

    if (state.board.at({Board::kSize - 1, Board::kSize - 1}).side == Side::Red) {
        return Side::Red;
    }
    if (state.board.at({0, 0}).side == Side::Blue) {
        return Side::Blue;
    }

    return Side::None;
}

}  // namespace core::rules