#include "rules.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace core::rules {

std::vector<int> get_selectable_ids(const GameState& state) {
    // 还没掷骰就没有可动棋子；返回空集合即可，调用方无需特判。
    if (!state.current_dice) {
        return {};
    }
    int dice = *state.current_dice;
    Side side = state.current_side;

    // 同号棋子在场时只能动它，没有降级空间。
    if (state.board.has_piece(side, dice)) {
        return {dice};
    }

    // 降级选择：先向下找"比点数小的最大编号"，再向上找"比点数大的最小编号"，
    // 两者可以同时存在（例如掷 3 点且场上只剩 1 号和 5 号）。
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
    if (state.current_side == Side::Red) {
        if (!((dx == 1 && dy == 0) || (dx == 0 && dy == 1) || (dx == 1 && dy == 1))) {
            return "红方只能向下、向右、向右下移动";
        }
    } else if (state.current_side == Side::Blue) {
        if (!((dx == -1 && dy == 0) || (dx == 0 && dy == -1) || (dx == -1 && dy == -1))) {
            return "蓝方只能向上、向左、向左上移动";
        }
    }

    // 到此为止没有检查 to 上有没有棋子：落点被占据是合法的，吃己方棋子同样合法。
    return "";
}

void apply_move(GameState& state, Pos from, Pos to) {
    // 再次校验而不是信任调用方：apply_move 是 public 接口，越界访问会直接崩。
    std::string err = check_move(state, from, to);
    if (!err.empty()) {
        throw std::invalid_argument(err);
    }

    // 直接覆盖落点（即吃子，可能是对方的也可能是己方的），再清空起点。
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