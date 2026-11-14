#include "board_widget.h"

#include <QPainter>

namespace ui {

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 400);  // 给它一个初始大小
}

void BoardWidget::update_state(const core::GameState& state) {
    current_state_ = state;
    update();  // 通知 Qt 重新调用 paintEvent
}

void BoardWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    // 这里下一回合再画，现在先画个背景测试
    painter.fillRect(rect(), QColor("#34495e"));

    // 随便画个圆测试
    painter.setBrush(Qt::red);
    painter.drawEllipse(50, 50, 80, 80);
}

}  // namespace ui