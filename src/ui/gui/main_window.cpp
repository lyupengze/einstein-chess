#include "main_window.h"

#include <QDebug>
#include <QVBoxLayout>

#include "setup_dialog.h"

namespace ui {

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    resize(500, 400);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    stacked_widget_ = new QStackedWidget(this);
    menu_widget_ = new MenuWidget(this);
    game_widget_ = new GameWidget(this);

    stacked_widget_->addWidget(menu_widget_);
    stacked_widget_->addWidget(game_widget_);

    layout->addWidget(stacked_widget_);

    connect(menu_widget_, &MenuWidget::start_game_clicked, this, [this]() {
        SetupDialog dialog(this);

        // exec() 会阻塞直到弹窗关闭
        if (dialog.exec() == QDialog::Accepted) {
            // 获取用户选择的颜色和数组
            int side_int = dialog.get_selected_side();
            core::Side human_side = (side_int == 1) ? core::Side::Red : core::Side::Blue;
            auto player_layout = dialog.get_layout();  // 改叫 player_layout 或 my_layout
            game_widget_->start_new_game(human_side, player_layout);
            stacked_widget_->setCurrentWidget(game_widget_);
        }
    });

    connect(game_widget_, &GameWidget::exit_to_menu, this,
            [this]() { stacked_widget_->setCurrentWidget(menu_widget_); });
}

}  // namespace ui