#include "dice_widget.h"

#include <QFont>
#include <QPainter>
#include <QString>
#include <algorithm>
#include <optional>

namespace ui {

// 骰子控件的绘制实现：白底圆角骰身 + 投影 + 深红色点阵，纯粹画几何图形，不产生随机数。
// 骰面取控件短边减去 20 像素（左右各留 10 像素）再居中，所以面板把它拉成什么比例都不会变形。
// 点阵由"圆心 + 偏移量 offset"组合而成：offset 是点中心相对骰面中心的横纵位移，
// 半径 r 控制点的大小，两者都以骰面边长 size 为基准，随控件缩放自动等比变化。

DiceWidget::DiceWidget(QWidget* parent) : QWidget(parent) {
    // 下限定 100：最小尺寸下六个点仍能分辨；上限 150：防止布局把它拉成一大块空白。
    setMinimumSize(100, 100);
    setMaximumSize(150, 150);
}

void DiceWidget::set_dice(std::optional<int> dice) {
    // 只缓存点数并请求重绘；update() 是异步合并的，同一事件循环内多次调用只会真正重绘一次。
    current_dice_ = dice;
    update();
}

void DiceWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 没有点数就整帧不画，控件退化为一块空白，视觉上等价于"本回合还没掷骰"。
    if (!current_dice_.has_value()) {
        return;
    }

    int dice_val = *current_dice_;
    // 短边减 20 像素后按差值居中，保证圆角与投影不会顶到控件边框。
    int size = std::min(width(), height()) - 20;
    int x = (width() - size) / 2;
    int y = (height() - size) / 2;

    QRect dice_rect(x, y, size, size);

    // 先画右下偏移 3 像素的半透明黑影，再让骰身把它盖住，得到"浮起"的立体感。
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 50));
    painter.drawRoundedRect(dice_rect.translated(3, 3), size / 5, size / 5);

    // 圆角半径取 size/5，与阴影保持一致；白色骰身配灰色描边。
    painter.setPen(QPen(QColor("#95a5a6"), 2));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(dice_rect, size / 5, size / 5);

    // 画点之前换成无边框画刷，否则每个点都会带一圈灰边。
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#c0392b"));

    // 点阵几何：r 是点的半径（size/10），offset 是点中心到骰面中心的横纵位移（size/4）。
    // 所有点数都用 (-offset, 0, +offset) 的组合表达：1 点在正中，2、3 沿主对角线，
    // 4、5、6 占四角，6 点再补上中间一列——即常见骰子的 6 点排布。
    int r = size / 10;
    int cx = dice_rect.center().x();
    int cy = dice_rect.center().y();
    int offset = size / 4;

    // 以骰面中心为原点画点，dx/dy 的正负号直接表示"右下 / 左上"。
    auto draw_dot = [&](int dx, int dy) { painter.drawEllipse(QPoint(cx + dx, cy + dy), r, r); };

    // 注意: 合法点数只会是 1~6；default 不是“没有骰子”，而是数据异常。
    // 这里画一个红色问号，避免非法值看起来和“本回合还没掷骰”完全一样。
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
        default:
            // 问号用与骰点相同的深红色，视觉上仍属于“骰子”而不是独立浮层。
            painter.setPen(QColor("#c0392b"));
            QFont font = painter.font();
            font.setBold(true);
            font.setPixelSize(size / 2);
            painter.setFont(font);
            painter.drawText(dice_rect, Qt::AlignCenter, QStringLiteral("?"));
            break;
    }
}

}  // namespace ui