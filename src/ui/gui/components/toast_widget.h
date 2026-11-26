#ifndef TOAST_WIDGET_H_
#define TOAST_WIDGET_H_

#include <QTimer>
#include <QWidget>

class QGraphicsOpacityEffect;
class QLabel;
class QPropertyAnimation;

namespace ui {

// 轻量浮动提示条：外层容器负责定位与淡入淡出，内层 QLabel 负责外观与阴影。
// 它不以窗口形式独立存在，而是挂在调用方（GameWidget）身上，靠 move() 绝对定位，
// 因而不会参与父控件的布局；显示位置只在 show_message() 里临时计算。

/// @brief 自动消失的提示条控件（QWidget 子类），外观由样式表中的 QLabel#toast 规则决定。
///
/// 典型用法：调用 show_message() 显示一句话，先淡入、停留 duration 毫秒、再淡出并隐藏，
/// 期间不需要调用方再操心任何事。
///
/// @note 透明度挂在容器上、阴影挂在文字标签上：一个控件同时只能有一个 QGraphicsEffect，
///       拆成两层才能既保留阴影又真正做淡入淡出（QWidget::setWindowOpacity 对子控件无效）。
class ToastWidget : public QWidget {
    Q_OBJECT
public:
    /// @brief 构造提示条并立即隐藏；阴影、透明度动画与单发定时器在这里一次性准备好。
    /// @param parent 父控件，同时也是本控件的定位参照系，不能为空
    explicit ToastWidget(QWidget* parent);

    /// @brief 显示一条提示，并重新开始倒计时。
    /// @param msg 提示文本，会原样显示（不自动换行，宽度由文本自然撑开）
    /// @param duration 完全显示的停留毫秒数，默认 2500；淡入淡出各占约 0.2 秒，不计入其中
    /// @note 重复调用会停掉进行中的动画与计时，从当前透明度重新淡入，不会出现两条提示叠加。
    void show_message(const QString& msg, int duration = 2500);

private:
    /// @brief 从当前透明度渐变到完全不透明；重复调用时从动画当前值接着走，不会先跳回 0。
    void fade_in();
    /// @brief 从当前透明度渐变到全透明，结束后隐藏；定时器超时时调用。
    void fade_out();

    QLabel* label_;                           ///< 文字与背景，样式表 QLabel#toast 的命中对象
    QGraphicsOpacityEffect* opacity_effect_;  ///< 挂在容器上的透明度效果，动画驱动它的 opacity
    QPropertyAnimation* animation_;           ///< 淡入/淡出共用一个动画，换方向时只改起止值
    QTimer* timer_;                           ///< 单发定时器：超时后开始淡出；父对象是本控件
    bool fading_out_ = false;                 ///< 动画结束时据此区分“淡入完”与“该隐藏了”
};

}  // namespace ui

#endif  // TOAST_WIDGET_H_