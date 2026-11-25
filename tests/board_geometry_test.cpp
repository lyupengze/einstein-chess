// 棋盘几何回归：BUG-01（右/下边缘余数像素点击导致越界坐标崩溃）。
//
// 旧实现用 board_size（= min(w,h)）做点击边界判定，用 cell_size * 5 画网格，
// 两者在小数除法下最多差 4 像素；落在余数条里的点击会算出列号 5 并抛
// std::out_of_range。这里直接对控件发鼠标事件，检查越界像素被忽略、
// 棋盘内像素仍能映射到正确行列。

#include <QApplication>
#include <QMouseEvent>
#include <iostream>
#include <string_view>

#include "components/board_widget.h"
#include "test_support.h"

using test_support::expect;

namespace {

// 发一次鼠标按下，返回是否产生了 cell_clicked，以及坐标。
// 默认左键；右键/中键用于验证棋盘只响应左键的回归。
bool click(ui::BoardWidget& widget, int x, int y, core::Pos& out,
           Qt::MouseButton button = Qt::LeftButton) {
    bool fired = false;
    const auto connection =
        QObject::connect(&widget, &ui::BoardWidget::cell_clicked, [&](core::Pos pos) {
            fired = true;
            out = pos;
        });

    const QPointF local(static_cast<double>(x), static_cast<double>(y));
    QMouseEvent event(QEvent::MouseButtonPress, local, local, button, button, Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);

    QObject::disconnect(connection);
    return fired;
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    ui::BoardWidget widget;
    // 402x402：board_size=402 -> cell_size=80 -> 网格 400，右侧/下侧各留 1 像素余量。
    widget.resize(402, 402);
    widget.show();
    app.processEvents();

    core::Pos pos{99, 99};

    expect(!click(widget, 401, 200, pos), "right remainder pixel is ignored");
    expect(!click(widget, 200, 401, pos), "bottom remainder pixel is ignored");
    expect(!click(widget, 0, 200, pos), "left padding is ignored");

    expect(click(widget, 400, 200, pos) && pos.x == 2 && pos.y == 4,
           "right-most board pixel maps to column 4");
    expect(click(widget, 1, 1, pos) && pos.x == 0 && pos.y == 0,
           "top-left board pixel maps to (0,0)");

    // 棋盘走子只响应左键：右键/中键不能误触；随后左键仍能正常工作。
    expect(!click(widget, 200, 200, pos, Qt::RightButton), "right click is ignored");
    expect(!click(widget, 200, 200, pos, Qt::MiddleButton), "middle click is ignored");
    expect(click(widget, 200, 200, pos) && pos.x == 2 && pos.y == 2,
           "left click still maps to the expected cell");

    // 非正方形窗口：留白在左右，判定与网格必须仍然一致。
    widget.resize(500, 402);
    app.processEvents();
    expect(!click(widget, 451, 200, pos), "pixel past right edge is ignored in wide window");
    expect(click(widget, 449, 200, pos) && pos.x == 2 && pos.y == 4,
           "last in-board column still maps to 4 in wide window");
    expect(!click(widget, 49, 200, pos), "left padding is ignored in wide window");

    widget.resize(402, 500);
    app.processEvents();
    expect(!click(widget, 200, 451, pos), "pixel past bottom edge is ignored in tall window");
    expect(click(widget, 200, 449, pos) && pos.x == 4 && pos.y == 2,
           "last in-board row still maps to 4 in tall window");

    return test_support::finish();
}
