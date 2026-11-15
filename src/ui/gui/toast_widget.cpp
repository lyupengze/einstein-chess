#include "toast_widget.h"

#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>

namespace ui {

ToastWidget::ToastWidget(QWidget* parent) : QLabel(parent) {
    setObjectName("toast");
    setAlignment(Qt::AlignCenter);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    hide();

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &ToastWidget::hide);
}

void ToastWidget::show_message(const QString& msg, int duration) {
    setText(msg);
    adjustSize();

    if (timer_->isActive()) {
        timer_->stop();
    }
    timer_->start(duration);

    if (parentWidget()) {
        int x = (parentWidget()->width() - width()) / 2;
        int y = parentWidget()->height() * 0.15;
        move(x, y);
    }

    setWindowOpacity(0.0);
    show();
    raise();

    auto* animation = new QPropertyAnimation(this, "windowOpacity", this);
    animation->setDuration(200);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

}  // namespace ui