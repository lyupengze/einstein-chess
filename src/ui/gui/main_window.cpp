#include "main_window.h"

#include <QDebug>
#include <QVBoxLayout>

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
        game_widget_->start_new_game();
        stacked_widget_->setCurrentWidget(game_widget_);
    });

    connect(game_widget_, &GameWidget::exit_to_menu, this,
            [this]() { stacked_widget_->setCurrentWidget(menu_widget_); });
}

}  // namespace ui