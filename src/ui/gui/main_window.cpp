#include "main_window.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>

#include "ai/ai.h"
#include "core/rules.h"

namespace ui {

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    resize(800, 600);

    auto* main_layout = new QVBoxLayout(this);
    auto* top_layout = new QHBoxLayout();
    auto* content_layout = new QHBoxLayout();

    status_label_ = new QLabel("等待开始游戏...", this);
    status_label_->setObjectName("statusLabel");

    message_label_ = new QLabel("", this);
    message_label_->setStyleSheet("color: #e74c3c; font-size: 14px; margin-left: 20px;");

    roll_btn_ = new QPushButton("掷骰子", this);
    start_btn_ = new QPushButton("开始新游戏", this);

    top_layout->addWidget(status_label_);
    top_layout->addWidget(message_label_);
    top_layout->addStretch();
    top_layout->addWidget(roll_btn_);
    top_layout->addWidget(start_btn_);
    main_layout->addLayout(top_layout);

    board_widget_ = new BoardWidget(this);
    dice_widget_ = new DiceWidget(this);

    content_layout->addWidget(board_widget_, 1);
    content_layout->addWidget(dice_widget_, 0, Qt::AlignTop);
    main_layout->addLayout(content_layout, 1);

    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::on_start_clicked);
    connect(roll_btn_, &QPushButton::clicked, this, &MainWindow::on_roll_dice_clicked);
    connect(board_widget_, &BoardWidget::cell_clicked, this, &MainWindow::on_cell_clicked);

    roll_btn_->setEnabled(false);
}

void MainWindow::on_start_clicked() {
    game_.prepare_new_game(core::Side::Red);

    game_.submit_layout(core::Side::Red, ai::decide_layout());
    game_.submit_layout(core::Side::Blue, ai::decide_layout());

    selected_pos_ = std::nullopt;
    update_ui();
}

void MainWindow::on_roll_dice_clicked() {
    try {
        game_.roll_dice();
        update_ui();
    } catch (const std::exception& e) {
        qDebug() << "掷骰子失败:" << e.what();
    }
}

void MainWindow::show_message(const QString& msg) {
    message_label_->setText("❌ " + msg);
    QTimer::singleShot(3000, this, [this]() { message_label_->setText(""); });
}

void MainWindow::on_cell_clicked(core::Pos pos) {
    auto state = game_.get_state();
    if (state.current_side != state.player1_side || state.phase != core::Phase::Player1Thinking) {
        return;
    }

    if (selected_pos_ && *selected_pos_ == pos) {
        selected_pos_ = std::nullopt;
        update_ui();
        return;
    }

    if (selected_pos_) {
        try {
            game_.make_move(*selected_pos_, pos);
            selected_pos_ = std::nullopt;
            update_ui();
            QTimer::singleShot(500, this, &MainWindow::handle_ai_turn);
            return;
        } catch (const std::exception& e) {
            auto piece = state.board.at(pos);
            if (piece.side == state.player1_side) {
                auto selectable = core::rules::get_selectable_ids(state);
                bool is_valid_to_select = false;
                for (int id : selectable) {
                    if (id == piece.id) {
                        is_valid_to_select = true;
                        break;
                    }
                }

                if (is_valid_to_select) {
                    selected_pos_ = pos;
                    update_ui();
                    return;
                }
            }
            selected_pos_ = std::nullopt;
            update_ui();
            show_message(QString("移动失败: ") + e.what());
        }
    }

    else {
        auto piece = state.board.at(pos);
        if (piece.side == state.player1_side) {
            auto selectable = core::rules::get_selectable_ids(state);
            bool is_valid_to_select = false;
            for (int id : selectable) {
                if (id == piece.id) {
                    is_valid_to_select = true;
                    break;
                }
            }

            if (is_valid_to_select) {
                selected_pos_ = pos;
                update_ui();
            } else {
                show_message("这个棋子本回合不能动！");
            }
        }
    }
}

void MainWindow::handle_ai_turn() {
    if (game_.get_state().phase == core::Phase::GameOver) {
        return;
    }
    if (game_.get_state().current_side == game_.get_state().player1_side) {
        return;
    }

    game_.roll_dice();
    ai::Move move = ai::decide_move(game_.get_state());
    game_.make_move(move.from, move.to);

    update_ui();
}

void MainWindow::update_ui() {
    auto state = game_.get_state();

    std::vector<core::Pos> selectable_pos;
    std::vector<core::Pos> reachable_pos;

    if (state.current_side == state.player1_side && state.phase == core::Phase::Player1Thinking) {
        if (!selected_pos_) {
            selectable_pos = get_selectable_positions();
        } else {
            reachable_pos = get_reachable_positions(*selected_pos_);
        }
    }

    board_widget_->update_state(state, selected_pos_, selectable_pos, reachable_pos);
    dice_widget_->set_dice(state.current_dice);

    if (state.phase == core::Phase::GameOver) {
        status_label_->setText(
            QString("%1 获胜！").arg(QString::fromStdString(core::side_name(state.winner))));
        roll_btn_->setEnabled(false);
        return;
    }

    if (state.current_side == state.player1_side) {
        if (state.phase == core::Phase::ThrowingDice) {
            status_label_->setText("轮到你了，请掷骰子");
            roll_btn_->setEnabled(true);
        } else {
            status_label_->setText("你掷出了 " + QString::number(*state.current_dice) + "，请走棋");
            roll_btn_->setEnabled(false);
        }
    } else {
        status_label_->setText("电脑思考中...");
        roll_btn_->setEnabled(false);
    }
}

std::vector<core::Pos> MainWindow::get_selectable_positions() const {
    std::vector<core::Pos> positions;
    auto state = game_.get_state();
    if (state.phase != core::Phase::Player1Thinking) {
        return positions;
    }

    auto selectable_ids = core::rules::get_selectable_ids(state);
    for (int id : selectable_ids) {
        auto pos = state.board.find_piece(state.player1_side, id);
        if (pos) {
            positions.push_back(*pos);
        }
    }
    return positions;
}

std::vector<core::Pos> MainWindow::get_reachable_positions(core::Pos from) const {
    std::vector<core::Pos> positions;
    auto state = game_.get_state();

    // 简单粗暴地遍历全图，用 check_move 判断
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            core::Pos to{x, y};
            if (core::rules::check_move(state, from, to).empty()) {
                positions.push_back(to);
            }
        }
    }
    return positions;
}

}  // namespace ui