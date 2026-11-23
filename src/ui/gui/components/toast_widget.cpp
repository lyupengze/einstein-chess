#include "toast_widget.h"

#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace ui {

namespace {

// 阴影模糊半径 12、向下偏移 4，容器四周至少留 16 像素透明边距，阴影才不会被控件边界裁掉。
constexpr int kShadowMargin = 16;
constexpr int kFadeInMs = 180;
constexpr int kFadeOutMs = 200;

}  // namespace

// 提示条实现：构造时备好阴影、透明度效果、动画与单发定时器并隐藏；
// show_message() 负责换文本、按新尺寸重新居中、淡入并重新计时，超时后淡出再隐藏。
// 由于它不进入父控件布局，位置只在每次 show_message() 时算一次，父控件之后缩放并不会跟随。
// 淡入淡出由 QGraphicsOpacityEffect 的 opacity 属性驱动——子控件上 setWindowOpacity 是空操作。

ToastWidget::ToastWidget(QWidget* parent) : QWidget(parent) {
    // 提示条只是反馈，不该拦住鼠标：真正需要点击的棋盘在它下面。
    setAttribute(Qt::WA_TransparentForMouseEvents);

    // 外层容器只负责定位、尺寸与透明度，自身不画任何东西；
    // 布局边距就是给内层标签的阴影预留的空间。
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);

    // 真正的外观（深色圆角背景、白字、内边距）由 style.qss 的 QLabel#toast 提供。
    label_ = new QLabel(this);
    label_->setObjectName("toast");
    label_->setAlignment(Qt::AlignCenter);
    label_->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(label_);

    // 阴影挂在标签上：effect 的父对象是标签，随控件树一起析构。
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 4);
    label_->setGraphicsEffect(shadow);

    // 透明度挂在容器上：一个控件同时只能有一个 QGraphicsEffect，阴影已占用标签的名额，
    // 所以必须分成两层，否则只能二选一。
    opacity_effect_ = new QGraphicsOpacityEffect(this);
    opacity_effect_->setOpacity(0.0);
    setGraphicsEffect(opacity_effect_);

    // 淡入与淡出共用一个动画：换方向时只改起止值与时长，避免同时有两个动画驱动同一个属性。
    animation_ = new QPropertyAnimation(opacity_effect_, "opacity", this);
    connect(animation_, &QPropertyAnimation::finished, this, [this]() {
        // 动画结束时只有“淡出”需要真正隐藏；淡入结束要保持可见。
        if (fading_out_) {
            fading_out_ = false;
            hide();
        }
    });

    // 构造即隐藏：子控件默认会随父控件一起显形，不先 hide() 就会在未定位时闪在左上角。
    hide();

    // 单发定时器：每次 show_message 重新 start，只负责“该开始淡出了”。
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &ToastWidget::fade_out);
}

void ToastWidget::show_message(const QString& msg, int duration) {
    // 先设文本再 adjustSize()，容器的尺寸由布局按标签的 sizeHint 重算；
    // 顺序颠倒会沿用上一条消息的宽度。
    label_->setText(msg);
    adjustSize();

    // 定位：横向按父子宽度差居中（阴影边距左右对称，视觉上仍是居中）；
    // 纵向取父控件高度的 15%，再减掉容器顶部的阴影边距，让“看得见的条”落在 15% 处。
    if (parentWidget()) {
        const int x = (parentWidget()->width() - width()) / 2;
        const int y = parentWidget()->height() * 3 / 20 - kShadowMargin;
        move(x, y);
    }

    // 已隐藏时把透明度归零，保证 show() 的第一帧是全透明，不会闪一下上一次的残留值。
    if (!isVisible()) {
        opacity_effect_->setOpacity(0.0);
    }
    raise();
    show();

    // 重置计时，让新消息拿到完整的 duration，否则上一条的 timeout 会提前把它收掉。
    timer_->start(duration);
    fade_in();
}

void ToastWidget::fade_in() {
    // 停掉可能正在进行的淡出：连续快速调用时，旧动画不能继续和新动画抢同一个属性。
    animation_->stop();
    fading_out_ = false;
    animation_->setDuration(kFadeInMs);
    animation_->setStartValue(opacity_effect_->opacity());
    animation_->setEndValue(1.0);
    animation_->start();
}

void ToastWidget::fade_out() {
    if (!isVisible()) {
        return;
    }
    fading_out_ = true;
    animation_->stop();
    animation_->setDuration(kFadeOutMs);
    animation_->setStartValue(opacity_effect_->opacity());
    animation_->setEndValue(0.0);
    animation_->start();
}

}  // namespace ui
