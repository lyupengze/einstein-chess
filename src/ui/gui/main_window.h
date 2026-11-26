/// @file src/ui/gui/main_window.h
/// @brief 主窗口：用页面栈在菜单页与对局页之间切换，并串起三条流程。

#pragma once

#include <QStackedWidget>
#include <QWidget>

#include "views/game_widget.h"
#include "views/menu_widget.h"

namespace gui {

/// @brief 应用主窗口，用 QStackedWidget 在菜单页与对局页之间切换。
///
/// 两个对话框（布阵、存档）与 storage::SaveManager 的使用都集中在本类与 GameWidget 里；
/// 页面自身只发信号、不弹窗，方便单独替换界面而不动流程。
class MainWindow : public QWidget {
    Q_OBJECT
public:
    /// @brief 创建两个页面并接好菜单的三条流程（新游戏 / 读档 / 返回主页）。
    /// @param parent 父控件，通常为 nullptr（顶层窗口）
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QStackedWidget* stacked_widget_ = nullptr;  ///< 页面栈，切页只换 currentWidget，不重建页面
    MenuWidget* menu_widget_ = nullptr;         ///< 菜单页，常驻栈中
    GameWidget* game_widget_ = nullptr;         ///< 对局页，常驻栈中，返回主页后 game_ 仍保留
};

}  // namespace gui
