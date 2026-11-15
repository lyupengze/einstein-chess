#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QStackedWidget>
#include <QWidget>

#include "views/game_widget.h"
#include "views/menu_widget.h"

// 主窗口（MainWindow）的接口声明。
// 它本身只是 QStackedWidget 的容器：第 0 页菜单、第 1 页对局，
// 并负责串起“菜单信号 -> 弹窗 -> 建局或载局 -> 切页”的流程。
namespace ui {

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
    QStackedWidget* stacked_widget_;  ///< 页面栈，切页只换 currentWidget，不重建页面
    MenuWidget* menu_widget_;         ///< 菜单页，常驻栈中
    GameWidget* game_widget_;         ///< 对局页，常驻栈中，返回主页后 game_ 仍保留
};

}  // namespace ui

#endif