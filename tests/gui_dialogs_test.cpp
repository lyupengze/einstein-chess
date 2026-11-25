// GUI 对话框回归：直接构造对话框、操作控件并调用私有槽，
// 大部分路径不进入模态事件循环，因此可以在 offscreen 平台上稳定运行。
//
// 覆盖四件事：
// 1. 布阵对话框的默认编号、点击循环、换边清空与合法确认；
// 2. 存档对话框的列表读取、默认选中、名称 trim 与确认路径；
// 3. 同名覆盖确认（No 保持对话框、Yes 才接受）；
// 4. 保存期间暂停 AI 定时器，确认/取消后都按剩余时间恢复。
//
// 注意：重复编号 / 空名称会弹出模态 QMessageBox，本用例刻意不触发这些分支，
// 避免测试进程卡在无人关闭的对话框上；同名覆盖确认与保存暂停则用 QTimer 自动操作。

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <array>
#include <cstdio>
#include <string>

#include "ai/ai.h"
#include "core/game.h"
#include "core/game_state.h"
#include "core/types.h"
#include "dialogs/save_load_dialog.h"
#include "dialogs/setup_dialog.h"
#include "qt_wait.h"
#include "storage/save_manager.h"
#include "views/game_widget.h"

#include "test_support.h"

using test_support::expect;

namespace {

/// @brief 取网格布局中 (x,y) 位置的按钮；越界或类型不符返回 nullptr。
QPushButton* grid_button_at(QGridLayout* grid, int x, int y) {
    if (grid == nullptr) {
        return nullptr;
    }
    QLayoutItem* item = grid->itemAtPosition(x, y);
    if (item == nullptr) {
        return nullptr;
    }
    return qobject_cast<QPushButton*>(item->widget());
}

/// @brief 按文本查找单选按钮，避免依赖控件创建顺序。
QRadioButton* find_radio(QWidget& root, const QString& needle) {
    for (QRadioButton* radio : root.findChildren<QRadioButton*>()) {
        if (radio->text().contains(needle)) {
            return radio;
        }
    }
    return nullptr;
}

/// @brief 判断对话框里是否存在包含指定文本的 QLabel。
bool has_label(QWidget& root, const QString& needle) {
    for (QLabel* label : root.findChildren<QLabel*>()) {
        if (label->text().contains(needle)) {
            return true;
        }
    }
    return false;
}

/// @brief 下一个模态 QMessageBox 出现时点击指定标准按钮，避免测试卡在 exec()。
void click_next_message_box(QMessageBox::StandardButton button) {
    QTimer::singleShot(0, [button]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box == nullptr) {
            return;
        }
        QAbstractButton* target = box->button(button);
        if (target != nullptr) {
            target->click();
        }
    });
}

/// @brief 按文本查找按钮，避免依赖控件创建顺序。
QPushButton* find_button(QWidget& root, const QString& text) {
    for (QPushButton* button : root.findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

/// @brief 造出“轮到蓝方(AI)思考”的真实局面，供保存暂停测试使用。
core::GameState make_ai_thinking_state() {
    core::Game game;
    game.prepare_new_game(core::Side::Red);
    game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
    game.submit_layout(core::Side::Blue, ai::decide_layout());
    game.roll_dice();
    const auto red_move = ai::decide_move(game.get_state());
    game.make_move(red_move.from, red_move.to);
    game.roll_dice();
    return game.get_state();
}

/// @brief 在保存对话框打开 delay_ms 后检查局面未推进，并确认或取消。
void drive_save_dialog(int delay_ms, QListWidget* history, const QString& name, bool accept) {
    QTimer::singleShot(delay_ms, [history, name, accept]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* candidate = qobject_cast<QDialog*>(widget);
                if (candidate != nullptr && candidate->isVisible()) {
                    dialog = candidate;
                    break;
                }
            }
        }
        expect(dialog != nullptr, "opening save pauses the game in a modal dialog");
        if (dialog == nullptr) {
            return;
        }

        // 若保存没有暂停定时器，AI 会在对话框背后走完一步，这里能直接抓到。
        expect(history == nullptr || history->count() == 0,
               "pending AI action does not run while the save dialog is open");

        if (!accept) {
            dialog->reject();
            return;
        }

        auto* input = dialog->findChild<QLineEdit*>();
        expect(input != nullptr, "save pause dialog has a name input");
        if (input == nullptr) {
            dialog->reject();
            return;
        }
        input->setText(name);
        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == QString::fromUtf8("确认")) {
                button->click();
                return;
            }
        }
        expect(false, "save pause dialog has a confirm button");
        dialog->reject();
    });
}

/// @brief 构造一个可序列化的最小局面，仅用于填充存档列表。
core::GameState make_minimal_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 1;
    state.board.at({0, 0}) = {core::Side::Red, 1};
    state.board.at({4, 4}) = {core::Side::Blue, 1};
    return state;
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 1. 布阵对话框：红方默认、编号 1~6 按区域顺序铺满、默认档位为 kDefaultLevel。
    {
        ui::SetupDialog dialog;
        expect(dialog.get_selected_side() == 1, "setup defaults to red");
        expect(dialog.get_selected_level() == ai::kDefaultLevel,
               "setup defaults to the default AI level");

        const std::array<int, 6> expected{1, 2, 3, 4, 5, 6};
        expect(dialog.get_layout() == expected, "setup fills 1..6 in region order");

        QGridLayout* grid = dialog.findChild<QGridLayout*>();
        expect(grid != nullptr, "setup has a grid layout");
        QPushButton* red_first = grid_button_at(grid, 0, 0);
        QPushButton* red_last = grid_button_at(grid, 2, 0);
        QPushButton* inactive = grid_button_at(grid, 2, 2);
        expect(red_first != nullptr && red_first->text() == "1",
               "setup red (0,0) shows number 1");
        expect(red_last != nullptr && red_last->text() == "6",
               "setup red (2,0) shows number 6");
        expect(inactive != nullptr && !inactive->isEnabled() && inactive->text().isEmpty(),
               "setup inactive cells are disabled and empty");
    }

    // 2. 点击活动格只让该格在 1~6 循环；点击非活动格被忽略。
    {
        ui::SetupDialog dialog;
        QGridLayout* grid = dialog.findChild<QGridLayout*>();
        expect(QMetaObject::invokeMethod(&dialog, "on_grid_clicked", Q_ARG(int, 0),
                                         Q_ARG(int, 0)),
               "setup grid click slot is invokable");

        const std::array<int, 6> after_first{2, 2, 3, 4, 5, 6};
        expect(dialog.get_layout() == after_first, "setup click cycles 1 to 2");

        QPushButton* button = grid_button_at(grid, 0, 0);
        expect(button != nullptr && button->text() == "2",
               "setup clicked button shows the new number");

        const std::array<int, 6> before_inactive = dialog.get_layout();
        expect(QMetaObject::invokeMethod(&dialog, "on_grid_clicked", Q_ARG(int, 2),
                                         Q_ARG(int, 2)),
               "inactive cell slot can be invoked");
        expect(dialog.get_layout() == before_inactive,
               "setup ignores clicks outside the active region");
    }

    // 3. 换到蓝方：旧编号清空，蓝方区域重新铺 1~6，红方按钮全部禁用。
    {
        ui::SetupDialog dialog;
        QRadioButton* blue = find_radio(dialog, QString::fromUtf8("蓝方"));
        expect(blue != nullptr, "setup has a blue radio");
        if (blue != nullptr) {
            blue->setChecked(true);
        }

        expect(dialog.get_selected_side() == 2, "setup switches to blue");
        const std::array<int, 6> expected{1, 2, 3, 4, 5, 6};
        expect(dialog.get_layout() == expected, "setup resets numbers when switching side");

        QGridLayout* grid = dialog.findChild<QGridLayout*>();
        QPushButton* blue_first = grid_button_at(grid, 4, 4);
        QPushButton* red_cell = grid_button_at(grid, 0, 0);
        expect(blue_first != nullptr && blue_first->text() == "1",
               "setup blue (4,4) shows number 1");
        expect(red_cell != nullptr && !red_cell->isEnabled(),
               "setup disables the old red region");

        expect(QMetaObject::invokeMethod(&dialog, "on_grid_clicked", Q_ARG(int, 4),
                                         Q_ARG(int, 4)),
               "blue grid click slot is invokable");
        expect(dialog.get_layout()[0] == 2, "setup blue click cycles 1 to 2");
    }

    // 4. 合法编号确认后对话框返回 Accepted。
    {
        ui::SetupDialog dialog;
        expect(QMetaObject::invokeMethod(&dialog, "on_confirm_clicked"),
               "setup confirm slot is invokable");
        expect(dialog.result() == QDialog::Accepted, "setup accepts a valid layout");
    }

    // 5. 存档对话框：空库提示、无选中项。
    std::remove(storage::kDefaultDbPath);
    {
        ui::SaveLoadDialog dialog(ui::DialogMode::Load);
        QListWidget* list = dialog.findChild<QListWidget*>();
        expect(list != nullptr && list->count() == 0, "load dialog starts with an empty list");
        expect(dialog.get_selected_name().empty(), "load dialog has no selection when empty");
        expect(has_label(dialog, QString::fromUtf8("没有任何存档")),
               "load dialog explains an empty database");
    }

    // 6. 预置一个存档后，读取模式应选中最近一条，确认后返回 Accepted。
    {
        storage::SaveManager manager(storage::kDefaultDbPath);
        manager.save_game("alpha", make_minimal_state(), ai::Level::kSearch);
    }
    {
        ui::SaveLoadDialog dialog(ui::DialogMode::Load);
        QListWidget* list = dialog.findChild<QListWidget*>();
        expect(list != nullptr && list->count() == 1, "load dialog lists the saved slot");
        expect(list != nullptr && list->currentRow() == 0, "load dialog preselects the first slot");
        expect(dialog.get_selected_name() == "alpha", "load dialog reports the selected name");

        expect(QMetaObject::invokeMethod(&dialog, "on_confirm_clicked"),
               "load confirm slot is invokable");
        expect(dialog.result() == QDialog::Accepted, "load dialog accepts a selected slot");
    }

    // 7. 保存模式：列表可见、名称 trim 后返回、确认返回 Accepted。
    {
        ui::SaveLoadDialog dialog(ui::DialogMode::Save);
        QListWidget* list = dialog.findChild<QListWidget*>();
        QLineEdit* input = dialog.findChild<QLineEdit*>();
        expect(list != nullptr && list->count() == 1, "save dialog lists existing slots");
        expect(input != nullptr, "save dialog has a name input");
        expect(has_label(dialog, QString::fromUtf8("同名将覆盖")),
               "save dialog warns about overwriting");

        if (input != nullptr) {
            input->setText("  my slot  ");
        }
        expect(dialog.get_save_name() == "my slot", "save dialog trims the slot name");

        expect(QMetaObject::invokeMethod(&dialog, "on_confirm_clicked"),
               "save confirm slot is invokable");
        expect(dialog.result() == QDialog::Accepted, "save dialog accepts a non-empty name");
    }

    // 8. 同名保存必须二次确认：选 No 保持对话框，选 Yes 才接受。
    {
        ui::SaveLoadDialog dialog(ui::DialogMode::Save);
        QLineEdit* input = dialog.findChild<QLineEdit*>();
        expect(input != nullptr, "overwrite test has a name input");
        if (input != nullptr) {
            input->setText("alpha");
        }
        click_next_message_box(QMessageBox::No);
        expect(QMetaObject::invokeMethod(&dialog, "on_confirm_clicked"),
               "overwrite confirm slot is invokable");
        expect(dialog.result() != QDialog::Accepted,
               "declining overwrite keeps the save dialog open");
    }
    {
        ui::SaveLoadDialog dialog(ui::DialogMode::Save);
        QLineEdit* input = dialog.findChild<QLineEdit*>();
        expect(input != nullptr, "overwrite accept test has a name input");
        if (input != nullptr) {
            input->setText("alpha");
        }
        click_next_message_box(QMessageBox::Yes);
        expect(QMetaObject::invokeMethod(&dialog, "on_confirm_clicked"),
               "overwrite accept slot is invokable");
        expect(dialog.result() == QDialog::Accepted,
               "confirming overwrite accepts the save dialog");
    }

    // 9. 保存期间暂停 AI 定时器：确认保存后按剩余时间恢复，AI 继续走完。
    {
        ui::GameWidget widget;
        widget.set_action_delays(1, 300, 300);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        auto* save = find_button(widget, QString::fromUtf8("保存"));
        expect(history != nullptr, "save pause test exposes the history list");
        expect(save != nullptr, "save pause test exposes the save button");
        widget.load_game_state(make_ai_thinking_state(), ai::kDefaultLevel);

        drive_save_dialog(900, history, "paused", true);
        if (save != nullptr) {
            save->click();  // 进入保存对话框；定时器应被暂停到对话框关闭
        }
        expect(test_support::wait_until(
                   [history] { return history != nullptr && history->count() == 1; }, 3000,
                   "AI resumes after the save dialog is accepted"),
               "accepting save restores the pending AI action");
    }

    // 10. 取消保存同样恢复，不能把这一手 AI 动作永久丢掉。
    {
        ui::GameWidget widget;
        widget.set_action_delays(1, 300, 300);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        auto* save = find_button(widget, QString::fromUtf8("保存"));
        expect(history != nullptr, "cancel pause test exposes the history list");
        expect(save != nullptr, "cancel pause test exposes the save button");
        widget.load_game_state(make_ai_thinking_state(), ai::kDefaultLevel);

        drive_save_dialog(900, history, QString(), false);
        if (save != nullptr) {
            save->click();
        }
        expect(test_support::wait_until(
                   [history] { return history != nullptr && history->count() == 1; }, 3000,
                   "AI resumes after the save dialog is cancelled"),
               "cancelling save restores the pending AI action");
    }

    std::remove(storage::kDefaultDbPath);
    return test_support::finish();
}
