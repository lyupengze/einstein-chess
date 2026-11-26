#ifndef MENU_WIDGET_H_
#define MENU_WIDGET_H_

#include <QPushButton>
#include <QWidget>

// 主菜单页（MenuWidget）的接口声明。
// 对外只暴露两个信号：start_game_clicked / load_game_clicked；
// 谁接收信号、弹哪个对话框、切到哪一页，全部在 main_window.cpp 里决定。
namespace ui {

/// @brief 主菜单页：标题、副标题与“开始新游戏 / 读取存档”两个入口。
///
/// 本类不持有任何对局数据，点击按钮只发信号，因此它能在 QStackedWidget 里被反复
/// 换入换出而不需要清理或重置。
class MenuWidget : public QWidget {
    Q_OBJECT
public:
    /// @brief 搭好标题、两个入口按钮与页脚版本号，并把点击翻译成信号。
    /// @param parent 父控件，生命周期交给 Qt 对象树
    explicit MenuWidget(QWidget* parent = nullptr);

signals:
    /// @brief 用户点了“开始新游戏”；接收方负责弹布阵对话框并切到对局页。
    void start_game_clicked();
    /// @brief 用户点了“读取存档”；接收方负责弹存档列表并载入选中的记录。
    void load_game_clicked();

private:
    QPushButton* start_btn_;  ///< 开始新游戏，沿用 style.qss 的蓝色主按钮配色
    QPushButton* load_btn_;   ///< 读取存档，单独压成灰底以弱化视觉权重
};

}  // namespace ui

#endif  // MENU_WIDGET_H_