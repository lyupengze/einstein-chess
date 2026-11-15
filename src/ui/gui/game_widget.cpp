#include "game_widget.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

#include "ai/ai.h"
#include "core/rules.h"
#include "save_load_dialog.h"
#include "storage/save_manager.h"

namespace ui {

GameWidget::GameWidget(QWidget* parent) : QWidget(parent) {
    auto* main_layout = new QVBoxLayout(this);
    auto* top_layout = new QHBoxLayout();
    auto* content_layout = new QHBoxLayout();
    auto* right_panel = new QVBoxLayout();

    status_label_ = new QLabel("等待开始游戏...", this);
    status_label_->setObjectName("statusLabel");

    roll_btn_ = new QPushButton("掷骰子", this);
    exit_btn_ = new QPushButton("返回主页", this);
    save_btn_ = new QPushButton("保存", this);
    roll_btn_->setFixedSize(100, 40);
    exit_btn_->setFixedSize(100, 40);
    save_btn_->setFixedSize(100, 40);

    top_layout->addWidget(status_label_);
    top_layout->addStretch();
    top_layout->addWidget(roll_btn_);
    top_layout->addWidget(exit_btn_);
    top_layout->addWidget(save_btn_);
    main_layout->addLayout(top_layout);

    board_widget_ = new BoardWidget(this);
    dice_widget_ = new DiceWidget(this);
    history_list_ = new QListWidget(this);
    history_list_->setObjectName("historyList");
    history_list_->setFixedWidth(200);

    right_panel->addWidget(dice_widget_, 0, Qt::AlignHCenter);
    right_panel->addWidget(history_list_);
    content_layout->addWidget(board_widget_, 1);
    content_layout->addLayout(right_panel);
    main_layout->addLayout(content_layout, 1);

    connect(roll_btn_, &QPushButton::clicked, this, &GameWidget::on_roll_dice_clicked);
    connect(exit_btn_, &QPushButton::clicked, this, &GameWidget::on_exit_clicked);
    connect(save_btn_, &QPushButton::clicked, this, &GameWidget::on_save_clicked);
    connect(board_widget_, &BoardWidget::cell_clicked, this, &GameWidget::on_cell_clicked);

    roll_btn_->setEnabled(false);

    toast_ = new ToastWidget(this);
    toast_->hide();
}

void GameWidget::start_new_game(core::Side human_side, const std::array<int, 6>& human_layout) {
    game_.prepare_new_game(human_side);

    game_.submit_layout(human_side, human_layout);

    core::Side ai_side = (human_side == core::Side::Red) ? core::Side::Blue : core::Side::Red;
    game_.submit_layout(ai_side, ai::decide_layout());

    selected_pos_ = std::nullopt;

    history_list_->clear();

    update_ui();
    trigger_ai_if_needed();
}

void GameWidget::load_game_state(const core::GameState& state) {
    game_.load_game(state);
    selected_pos_ = std::nullopt;

    history_list_->clear();

    update_ui();
    trigger_ai_if_needed();
}

void GameWidget::on_exit_clicked() {
    emit exit_to_menu();
}

void GameWidget::on_roll_dice_clicked() {
    roll_btn_->setEnabled(false);
    status_label_->setText("🎲 正在掷骰子...");

    QTimer::singleShot(400, this, [this]() {
        try {
            game_.roll_dice();
            update_ui();
        } catch (const std::exception& e) {
            qDebug() << "掷骰子失败:" << e.what();
        }
    });
}

void GameWidget::on_save_clicked() {
    SaveLoadDialog dialog(DialogMode::Save, this);
    if (dialog.exec() == QDialog::Accepted) {
        std::string name = dialog.get_save_name();
        try {
            storage::SaveManager save_manager("einstein_save.db");
            save_manager.save_game(name, game_.get_state());
            show_message("游戏已保存！");
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "保存失败", e.what());
        }
    }
}

void GameWidget::show_message(const QString& msg) {
    toast_->show_message(msg);
}

void GameWidget::add_history_log(const QString& text) {
    history_list_->addItem(text);
    history_list_->scrollToBottom();
}

void GameWidget::on_cell_clicked(core::Pos pos) {
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

            auto target_piece = state.board.at(pos);
            QString log = QString("你: (%1,%2)->(%3,%4)")
                              .arg(selected_pos_->x)
                              .arg(selected_pos_->y)
                              .arg(pos.x)
                              .arg(pos.y);
            if (target_piece.side != core::Side::None) {
                log += QString(" 吃 %1%2")
                           .arg(target_piece.side == core::Side::Red ? "R" : "B")
                           .arg(target_piece.id);
            }

            add_history_log(log);
            update_ui();

            trigger_ai_if_needed();
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
    } else {
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

void GameWidget::trigger_ai_if_needed() {
    auto state = game_.get_state();

    if (state.phase == core::Phase::GameOver) {
        return;
    }
    if (state.current_side == state.player1_side) {
        return;
    }

    QTimer::singleShot(500, this, &GameWidget::handle_ai_turn);
}

void GameWidget::handle_ai_turn() {
    auto state = game_.get_state();
    if (state.phase == core::Phase::GameOver || state.current_side == state.player1_side) {
        return;
    }

    if (state.phase == core::Phase::ThrowingDice) {
        game_.roll_dice();
        update_ui();

        QTimer::singleShot(800, this, [this]() {
            try {
                auto move = ai::decide_move(game_.get_state());

                auto target_piece = game_.get_state().board.at(move.to);
                QString log = QString("电脑: (%1,%2)->(%3,%4)")
                                  .arg(move.from.x)
                                  .arg(move.from.y)
                                  .arg(move.to.x)
                                  .arg(move.to.y);
                if (target_piece.side != core::Side::None) {
                    log += QString(" 吃 %1%2")
                               .arg(target_piece.side == core::Side::Red ? "R" : "B")
                               .arg(target_piece.id);
                }
                add_history_log(log);

                game_.make_move(move.from, move.to);
                update_ui();
            } catch (const std::exception& e) {
                qDebug() << "AI 异常:" << e.what();
            }
        });
    }
}

void GameWidget::update_ui() {
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
            QString("🏆 %1 获胜！").arg(QString::fromStdString(core::side_name(state.winner))));
        roll_btn_->setEnabled(false);
        return;
    }

    if (state.current_side == state.player1_side) {
        if (state.phase == core::Phase::ThrowingDice) {
            status_label_->setText("轮到你了，请掷骰子");
            roll_btn_->setEnabled(true);
        } else {
            status_label_->setText("🎲 你掷出了 " + QString::number(*state.current_dice) +
                                   "，请走棋");
            roll_btn_->setEnabled(false);
        }
    } else {
        if (state.phase == core::Phase::ThrowingDice) {
            status_label_->setText("🤖 电脑正在掷骰子...");
        } else {
            status_label_->setText("🎲 电脑掷出了 " + QString::number(*state.current_dice) +
                                   "，正在思考...");
        }
        roll_btn_->setEnabled(false);
    }
}

std::vector<core::Pos> GameWidget::get_selectable_positions() const {
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

std::vector<core::Pos> GameWidget::get_reachable_positions(core::Pos from) const {
    std::vector<core::Pos> positions;
    auto state = game_.get_state();

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