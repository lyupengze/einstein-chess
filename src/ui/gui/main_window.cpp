#include "main_window.h"

#include <QDebug>
#include <QMessageBox>
#include <QVBoxLayout>

#include "save_load_dialog.h"
#include "setup_dialog.h"
#include "storage/save_manager.h"

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

        if (dialog.exec() == QDialog::Accepted) {
            int side_int = dialog.get_selected_side();
            core::Side human_side = (side_int == 1) ? core::Side::Red : core::Side::Blue;
            auto player_layout = dialog.get_layout();
            game_widget_->start_new_game(human_side, player_layout);
            stacked_widget_->setCurrentWidget(game_widget_);
        }
    });

    connect(menu_widget_, &MenuWidget::load_game_clicked, this, [this]() {
        SaveLoadDialog dialog(DialogMode::Load, this);
        if (dialog.exec() == QDialog::Accepted) {
            std::string name = dialog.get_selected_name();
            try {
                storage::SaveManager save_manager("einstein_save.db");
                auto state_opt = save_manager.load_game(name);

                if (state_opt) {
                    game_widget_->load_game_state(*state_opt);
                    stacked_widget_->setCurrentWidget(game_widget_);
                } else {
                    QMessageBox::warning(this, "错误", "找不到该存档。");
                }
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "读取失败", e.what());
            }
        }
    });

    connect(game_widget_, &GameWidget::exit_to_menu, this,
            [this]() { stacked_widget_->setCurrentWidget(menu_widget_); });
}

}  // namespace ui