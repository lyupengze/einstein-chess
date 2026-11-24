#include "main_window.h"

#include <QMessageBox>
#include <QVBoxLayout>
#include <exception>
#include <string>

#include "dialogs/save_load_dialog.h"
#include "dialogs/setup_dialog.h"
#include "storage/save_manager.h"

// 主窗口实现：搭 QStackedWidget，再把菜单的两个信号接到两个 lambda 上。
// 用 lambda 而不用成员槽，是因为这两段流程都只连接一次，且需要就地捕获 this；
// 对话框一律以 this 为父，模态执行期间主窗口不响应输入。
namespace ui {

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
            // 注意: 不要改成 static_cast<Side>(side_int)——那才会依赖枚举的数值顺序。
            int side_int = dialog.get_selected_side();
            core::Side human_side = (side_int == 1) ? core::Side::Red : core::Side::Blue;
            auto player_layout = dialog.get_layout();
            // start_new_game 内部会同时提交人类与 AI 的布阵，返回时阶段已在 ThrowingDice。
            // 强度与布阵一起传入：对话框默认选 kDefaultLevel（最新版），整局沿用。
            game_widget_->start_new_game(human_side, player_layout, dialog.get_selected_level());
            stacked_widget_->setCurrentWidget(game_widget_);
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
            std::string name = dialog.get_selected_name();
            // 每次新建：数据库连接的生命周期就锁在这个 lambda 里。
            storage::SaveManager save_manager(storage::kDefaultDbPath);
            auto saved = save_manager.load_game(name);

            if (saved) {
                // 强度取自存档本身，而不是当前菜单/上一次新局的选择：
                // 同一个存档无论读多少次、中间开过什么强度的新局，电脑强度都不变。
                game_widget_->load_game_state(saved->state, saved->ai_level);
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

    // 流程三：对局页的“返回主页”。不清理 game_，也不停 AI 定时器：切页瞬间若已有
    // AI 行动排队，它仍会在后台把这一步走完（返回主页不暂停对局）。
    // 真正危险的“旧回调落到新局”由 GameWidget::start_new_game / load_game_state
    // 里的 cancel_pending_actions() 兜住。
    connect(game_widget_, &GameWidget::exit_to_menu, this,
            [this]() { stacked_widget_->setCurrentWidget(menu_widget_); });
}

}  // namespace ui