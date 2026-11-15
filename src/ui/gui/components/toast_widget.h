#ifndef TOAST_WIDGET_H_
#define TOAST_WIDGET_H_

#include <QLabel>
#include <QTimer>

namespace ui {

// 轻量浮动提示条：一个自带圆角深色背景的 QLabel，用来短暂显示"游戏已保存！"这类反馈。
// 它不以窗口形式独立存在，而是挂在调用方（GameWidget）身上，靠 move() 绝对定位，
// 因而不会参与父控件的布局；显示位置、居中与淡入都在 show_message() 里临时计算。

/// @brief 自动消失的提示条控件（QLabel 子类），外观由样式表中的 #toast 规则决定。
///
/// 典型用法：调用 show_message() 显示一句话，duration 毫秒后自己隐藏，
/// 期间不需要调用方再操心任何事。
class ToastWidget : public QLabel {
    Q_OBJECT
public:
    /// @brief 构造提示条并立即隐藏；阴影与单发定时器在这里一次性准备好。
    /// @param parent 父控件，同时也是本控件的定位参照系，不能为空
    explicit ToastWidget(QWidget* parent);

    /// @brief 显示一条提示，并重新开始倒计时。
    /// @param msg 提示文本，会原样显示（不自动换行，宽度由文本自然撑开）
    /// @param duration 停留毫秒数，默认 2500；从调用时起算
    /// @note 重复调用会先停掉旧计时再重新计时，并从全透明重新淡入，不会出现两条提示叠加。
    void show_message(const QString& msg, int duration = 2500);

private:
    QTimer* timer_;  ///< 单发定时器：超时即隐藏；父对象是本控件，随控件一同析构
};

}  // namespace ui

#endif