#include "toast_widget.h"

#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>

namespace ui {

// 提示条实现：构造时备好阴影、单发定时器并隐藏；show_message() 负责换文本、重新计时、
// 定位到父控件 15% 高度处水平居中，最后从头淡入。
// 由于它不进入父控件布局，位置只在每次 show_message() 时算一次，父控件之后缩放并不会跟随。
// 淡入动画每次新建、以 DeleteWhenStopped 交给 Qt 回收，所以本类无需保存动画指针。

ToastWidget::ToastWidget(QWidget* parent) : QLabel(parent) {
    // objectName 是样式表的命中锚点：style.qss 的 QLabel#toast 提供深色背景、圆角与内边距。
    setObjectName("toast");
    setAlignment(Qt::AlignCenter);

    // 阴影挂在本控件上，让深色圆角块显得有厚度；effect 的父对象是 this，随控件一起析构。
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    // 构造即隐藏：子控件默认会随父控件一起显形，不先 hide() 就会在未定位时闪在左上角。
    hide();

    // 单发定时器：每次 show_message 重新 start，只会超时一次就隐藏，无需周期性回调。
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &ToastWidget::hide);
}

void ToastWidget::show_message(const QString& msg, int duration) {
    // 先设文本再 adjustSize()，尺寸才按新文本重算；顺序颠倒会沿用上一条消息的宽度。
    setText(msg);
    adjustSize();

    // 重置计时，让新消息拿到完整的 duration，否则上一条的 timeout 会提前把它收掉。
    // start() 对运行中的定时器本身就会重新计时，这里的 stop 只是把"重置"意图写明。
    if (timer_->isActive()) {
        timer_->stop();
    }
    timer_->start(duration);

    // 定位：横向按父子宽度差居中，纵向取父控件高度的 15%，即"靠上但不贴顶"。
    // 注意: 高度用的是父控件整体高度且与布局无关，父控件之后缩放时本控件不会跟随移动。
    if (parentWidget()) {
        int x = (parentWidget()->width() - width()) / 2;
        int y = parentWidget()->height() * 0.15;
        move(x, y);
    }

    // 先置全透明再显示，避免"满不透明闪一帧"再淡入的跳变；raise() 保证压在棋盘之上。
    // 注意: windowOpacity 只对顶层窗口生效，本控件是子控件，这段淡入实际很可能不起作用。
    setWindowOpacity(0.0);
    show();
    raise();

    // 动画每次新建：QPropertyAnimation 的父对象是 this，同时以 DeleteWhenStopped 让它在停止后
    // 自删——父子关系与自删并不冲突，QObject 析构时会自动从父对象列表中摘除自己。
    // 只有淡入没有淡出：到时由定时器直接 hide()，所以消失是硬切而不是渐变。
    // 连续快速调用时旧动画不会被 stop，会与新动画同时驱动同一个属性（终点相同，影响有限）。
    auto* animation = new QPropertyAnimation(this, "windowOpacity", this);
    animation->setDuration(200);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

}  // namespace ui