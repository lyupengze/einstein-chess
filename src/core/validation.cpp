#include "validation.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "board.h"
#include "rules.h"
#include "types.h"

namespace core {

namespace {

/// @brief 判断阵营字段是否落在枚举定义的值域内（含 None）。
bool is_known_side(Side side) {
    return side == Side::None || side == Side::Red || side == Side::Blue;
}

/// @brief 判断某阵营是否是可以实际行棋的一方。
bool is_playable_side(Side side) {
    return side == Side::Red || side == Side::Blue;
}

/// @brief 判断阶段字段是否落在枚举定义的值域内。
bool is_known_phase(Phase phase) {
    return phase == Phase::LayOut || phase == Phase::ThrowingDice ||
           phase == Phase::Player1Thinking || phase == Phase::Player2Thinking ||
           phase == Phase::GameOver;
}

/// @brief 向问题列表追加一条记录。
void add_issue(std::vector<StateIssue>& issues, std::string field, std::string message) {
    issues.push_back(StateIssue{std::move(field), std::move(message)});
}

/// @brief 校验棋盘格：空格必须 id=0；棋子必须是红/蓝且 id 在 1..6，同方不重复。
/// @param issues 收集问题
/// @param board 待校验棋盘
void validate_board(std::vector<StateIssue>& issues, const Board& board) {
    std::array<std::array<bool, Board::kChessCount + 1>, 2> seen{};

    for (std::size_t x = 0; x < Board::kSize; ++x) {
        for (std::size_t y = 0; y < Board::kSize; ++y) {
            const Piece& piece = board.at({x, y});

            if (piece.side == Side::None) {
                if (piece.id != 0) {
                    add_issue(issues, "board", "空格上的棋子编号必须为 0");
                }
                continue;
            }

            if (!is_playable_side(piece.side)) {
                add_issue(issues, "board", "棋盘上出现了未知阵营的棋子");
                continue;
            }

            if (piece.id < 1 || piece.id > Board::kChessCount) {
                add_issue(issues, "board", "棋子编号必须在 1~6 之间");
                continue;
            }

            const std::size_t side_index = piece.side == Side::Red ? 0 : 1;
            const std::size_t id_index = static_cast<std::size_t>(piece.id);
            if (seen[side_index][id_index]) {
                add_issue(issues, "board", "同一方出现了重复的棋子编号");
            } else {
                seen[side_index][id_index] = true;
            }
        }
    }

    if (board.count_pieces(Side::Red) > Board::kChessCount) {
        add_issue(issues, "board", "红方棋子数不能超过 6");
    }
    if (board.count_pieces(Side::Blue) > Board::kChessCount) {
        add_issue(issues, "board", "蓝方棋子数不能超过 6");
    }
}

}  // namespace

std::vector<StateIssue> validate_state(const GameState& state) {
    std::vector<StateIssue> issues;

    // 1. 先检查枚举值域，避免后面把越界值当作已知阵营使用。
    if (!is_known_phase(state.phase)) {
        add_issue(issues, "phase", "阶段值不在枚举范围内");
    }
    if (!is_known_side(state.player1_side)) {
        add_issue(issues, "player1_side", "发起方阵营不在枚举范围内");
    } else if (!is_playable_side(state.player1_side)) {
        add_issue(issues, "player1_side", "发起方必须是红方或蓝方");
    }
    if (!is_known_side(state.current_side)) {
        add_issue(issues, "current_side", "当前方阵营不在枚举范围内");
    }
    if (!is_known_side(state.winner)) {
        add_issue(issues, "winner", "胜者阵营不在枚举范围内");
    }

    // 2. 阶段与 current_side / current_dice 的对应关系。
    const bool thinking =
        state.phase == Phase::Player1Thinking || state.phase == Phase::Player2Thinking;

    switch (state.phase) {
        case Phase::LayOut:
            // 布阵阶段 current_side 通常为 None；允许空棋盘与未就绪。
            break;
        case Phase::ThrowingDice:
            if (!is_playable_side(state.current_side)) {
                add_issue(issues, "current_side", "掷骰阶段必须有且只有红/蓝当前方");
            }
            break;
        case Phase::Player1Thinking:
            if (state.current_side != state.player1_side) {
                add_issue(issues, "current_side", "Player1Thinking 阶段必须轮到 player1_side");
            }
            break;
        case Phase::Player2Thinking:
            if (!is_playable_side(state.current_side) ||
                state.current_side == state.player1_side) {
                add_issue(issues, "current_side",
                          "Player2Thinking 阶段必须轮到另一方可实际行棋的阵营");
            }
            break;
        case Phase::GameOver:
            // 终局 current_side 只作展示用途，允许 None。
            break;
    }

    // 3. 骰子：只有思考阶段允许有值，且必须是 1~6。
    if (state.current_dice.has_value()) {
        const int dice = *state.current_dice;
        if (dice < 1 || dice > Board::kChessCount) {
            add_issue(issues, "current_dice", "骰子点数必须在 1~6 之间");
        }
        if (!thinking) {
            add_issue(issues, "current_dice", "只有思考阶段可以携带骰子点数");
        }
    } else if (thinking) {
        add_issue(issues, "current_dice", "思考阶段必须有骰子点数");
    }

    // 4. winner 与阶段：未终局不能有胜者，终局必须有胜者。
    if (state.phase == Phase::GameOver) {
        if (!is_playable_side(state.winner)) {
            add_issue(issues, "winner", "终局必须记录红方或蓝方胜者");
        }
    } else if (state.winner != Side::None) {
        add_issue(issues, "winner", "非终局阶段不能记录胜者");
    }

    // 5. 棋盘本身的数据合法性。
    validate_board(issues, state.board);

    const int red_count = state.board.count_pieces(Side::Red);
    const int blue_count = state.board.count_pieces(Side::Blue);

    // 6. 棋盘与阶段/胜负的一致性。
    if (state.phase == Phase::LayOut) {
        // 布阵阶段允许空棋盘、部分棋子；此时 check_winner 对"双方都 0 子"会误判，
        // 所以不能在这里调用它。
    } else if (state.phase == Phase::GameOver) {
        if (red_count == 0 && blue_count == 0) {
            add_issue(issues, "board", "终局不能双方都没有棋子");
        }
        const Side actual = rules::check_winner(state);
        if (is_playable_side(state.winner) && actual != state.winner) {
            add_issue(issues, "winner", "记录的胜者与棋盘上的胜负条件不一致");
        }
    } else {
        if (red_count == 0 || blue_count == 0) {
            add_issue(issues, "board", "非终局阶段双方都必须至少有一枚棋子");
        }
        const Side actual = rules::check_winner(state);
        if (actual != Side::None) {
            add_issue(issues, "winner", "棋盘已经满足胜利条件，但阶段不是 GameOver");
        }
    }

    return issues;
}

void require_valid_state(const GameState& state) {
    const std::vector<StateIssue> issues = validate_state(state);
    if (issues.empty()) {
        return;
    }

    std::string message = "局面状态不合法";
    for (const StateIssue& issue : issues) {
        message += "\n- ";
        message += issue.field;
        message += ": ";
        message += issue.message;
    }
    throw std::invalid_argument(message);
}

}  // namespace core
