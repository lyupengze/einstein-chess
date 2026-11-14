#include "main_window.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace ui {

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    resize(800, 600);

    auto* main_layout = new QVBoxLayout(this);

    auto* top_layout = new QHBoxLayout();
    status_label_ = new QLabel("等待开始游戏...", this);
    status_label_->setObjectName("statusLabel");
    auto* start_btn = new QPushButton("开始新游戏", this);

    top_layout->addWidget(status_label_);
    top_layout->addStretch();
    top_layout->addWidget(start_btn);
    main_layout->addLayout(top_layout);

    board_widget_ = new BoardWidget(this);
    main_layout->addWidget(board_widget_, 1);

    game_.prepare_new_game(core::Side::Red);
    game_.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
    game_.submit_layout(core::Side::Blue, {6, 5, 4, 3, 2, 1});
    board_widget_->update_state(game_.get_state());
}

}  // namespace ui