/// @file src/ui/gui/main_window.cpp
/// @brief 搭页面栈并接好菜单的三条流程：新游戏、读档、返回主页。
///
/// 两段菜单流程用 lambda 而非成员槽：只连接一次且需要就地捕获 this。
/// 对话框一律以 this 为父，模态执行期间主窗口不响应输入。

#include "main_window.h"

#include <QMessageBox>
#include <QVBoxLayout>
#include <exception>
#include <string>

#include "dialogs/save_load_dialog.h"
#include "dialogs/setup_dialog.h"
#include "storage/save_manager.h"

namespace gui {

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    // 500x400 是菜单页的初始尺寸；对局页靠布局与棋盘的最小尺寸自己撑大窗口。
    resize(500, 400);

    // 外边距清零，让栈里的页面铺满整窗，背景渐变才不会被露出边框。
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    stacked_widget_ = new QStackedWidget(this);
    menu_widget_ = new MenuWidget(this);
    game_widget_ = new GameWidget(this);

    // 两个页面都常驻，切页只改 currentWidget；因此对局页的 game_ 在返回主页后依然存在。
    stacked_widget_->addWidget(menu_widget_);
    stacked_widget_->addWidget(game_widget_);

    layout->addWidget(stacked_widget_);

    // 流程一：开始新游戏。布阵对话框 Accepted 才继续，取消则留在菜单页。
    connect(menu_widget_, &MenuWidget::start_game_clicked, this, [this]() {
        SetupDialog dialog(this);

        if (dialog.exec() == QDialog::Accepted) {
            // get_selected_side 的约定是 1=红、2=蓝，这里按 1/2 显式映射；
            // 注意：不要写成 static_cast<Side>(side_int)——那会依赖枚举的数值顺序。
            const int side_int = dialog.get_selected_side();
            const core::Side human_side = (side_int == 1) ? core::Side::Red : core::Side::Blue;
            const auto player_layout = dialog.get_layout();
            // start_new_game 内部会同时提交人类与 AI 的布阵，返回时阶段已在 ThrowingDice。
            // 档位与布阵一起传入：对话框默认选 kDefaultLevel，整局沿用。
            try {
                game_widget_->start_new_game(human_side, player_layout,
                                             dialog.get_selected_level());
                stacked_widget_->setCurrentWidget(game_widget_);
            } catch (const std::exception& e) {
                // 布阵或开局状态不合法时留在菜单页，避免界面停在一个半初始化的对局上。
                QMessageBox::critical(this, "开局失败", e.what());
            }
        }
    });

    // 流程二：读取存档。清空 selected_pos_ 与历史、覆盖式载入，然后同样切到对局页。
    connect(menu_widget_, &MenuWidget::load_game_clicked, this, [this]() {
        // 对话框构造时就会打开数据库，所以整段（含构造）都在 try 内，
        // 避免异常穿出 Qt 事件循环把进程直接终止。
        try {
            SaveLoadDialog dialog(DialogMode::Load, this);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            const auto name = dialog.get_selected_name();
            if (!name) {
                return;
            }
            // 每次新建：数据库连接的生命周期就锁在这个 lambda 里。
            storage::SaveManager save_manager(storage::kDefaultDbPath);
            auto saved = save_manager.load_game(*name);

            if (saved) {
                // 档位取自存档本身，而不是当前菜单或上一次新局的选择：
                // 同一个存档无论读多少次、中间开过什么档位的新局，电脑档位都不变。
                game_widget_->load_game_state(saved->state, saved->ai_level, saved->rng);
                stacked_widget_->setCurrentWidget(game_widget_);
            } else {
                // 记录可能在对话框打开后已被删除，所以查不到不算异常。
                QMessageBox::warning(this, "错误", "找不到该存档。");
            }
        } catch (const std::exception& e) {
            // 数据库打不开或反序列化对损坏数据抛异常，报错并留在菜单页。
            QMessageBox::critical(this, "读取失败", e.what());
        }
    });

    // 流程三：对局页的“返回主页”。GameWidget 会先停掉尚未执行的掷骰/AI 定时器，
    // 再发出信号；因此切回菜单后，后台不会继续推进一个玩家已经离开的局面。
    // game_ 本身保留在内存里，等下次开新局/读档时整体覆盖。
    connect(game_widget_, &GameWidget::exit_to_menu, this,
            [this]() { stacked_widget_->setCurrentWidget(menu_widget_); });
}

}  // namespace gui
