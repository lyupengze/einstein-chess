#include "dice_widget.h"

#include <QPainter>

namespace ui {

DiceWidget::DiceWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(100, 100);  // 给骰子预留空间
    setMaximumSize(150, 150);
}

void DiceWidget::set_dice(std::optional<int> dice) {
    current_dice_ = dice;
    update();
}

void DiceWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!current_dice_.has_value()) {
        return;  // 没点数就不画
    }

    int dice_val = *current_dice_;
    int size = std::min(width(), height()) - 20;  // 留点边距
    int x = (width() - size) / 2;
    int y = (height() - size) / 2;

    QRect dice_rect(x, y, size, size);

    // 画白色圆角底色带阴影
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 50));
    painter.drawRoundedRect(dice_rect.translated(3, 3), size / 5, size / 5);  // 阴影

    painter.setPen(QPen(QColor("#95a5a6"), 2));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(dice_rect, size / 5, size / 5);

    // 画点数
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#c0392b"));

    int r = size / 10;
    int cx = dice_rect.center().x();
    int cy = dice_rect.center().y();
    int offset = size / 4;

    auto draw_dot = [&](int dx, int dy) { painter.drawEllipse(QPoint(cx + dx, cy + dy), r, r); };

    switch (dice_val) {
        case 1:
            draw_dot(0, 0);
            break;
        case 2:
            draw_dot(-offset, -offset);
            draw_dot(offset, offset);
            break;
        case 3:
            draw_dot(-offset, -offset);
            draw_dot(0, 0);
            draw_dot(offset, offset);
            break;
        case 4:
            draw_dot(-offset, -offset);
            draw_dot(offset, -offset);
            draw_dot(-offset, offset);
            draw_dot(offset, offset);
            break;
        case 5:
            draw_dot(-offset, -offset);
            draw_dot(offset, -offset);
            draw_dot(0, 0);
            draw_dot(-offset, offset);
            draw_dot(offset, offset);
            break;
        case 6:
            draw_dot(-offset, -offset);
            draw_dot(offset, -offset);
            draw_dot(-offset, 0);
            draw_dot(offset, 0);
            draw_dot(-offset, offset);
            draw_dot(offset, offset);
            break;
    }
}

}  // namespace ui