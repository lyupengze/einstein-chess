/// @file tests/board_geometry_test.cpp
/// @brief 棋盘几何回归：边缘余数像素不得换算成越界坐标。
///
/// 网格覆盖边长是 cell_size * 5，而控件边长 board_size = min(w, h) 在小数除法下
/// 最多多出 4 像素；若用 board_size 判边界，落在余数条里的点击会算出下标 5 并
/// 抛 std::out_of_range。这里直接对控件发鼠标事件，检查越界像素被忽略、棋盘内
/// 像素仍映射到正确行列。背景见 `#board-remainder-click`。

#include <QApplication>
#include <QMouseEvent>
#include <iostream>
#include <string_view>

#include "components/board_widget.h"
#include "test_support.h"

using test_support::expect;

namespace {

/// @brief 发一次鼠标按下，返回是否产生 cell_clicked 与坐标。
///
/// 默认左键；右键与中键用于验证棋盘只响应左键。
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

    expect(!click(widget, 401, 200, pos), "右边缘余数像素被忽略");
    expect(!click(widget, 200, 401, pos), "下边缘余数像素被忽略");
    expect(!click(widget, 0, 200, pos), "左侧留白被忽略");

    expect(click(widget, 400, 200, pos) && pos.x == 2 && pos.y == 4, "棋盘最右像素映射到第 4 列");
    expect(click(widget, 1, 1, pos) && pos.x == 0 && pos.y == 0, "棋盘左上像素映射到 (0,0)");

    // 棋盘走子只响应左键：右键/中键不能误触；随后左键仍能正常工作。
    expect(!click(widget, 200, 200, pos, Qt::RightButton), "右键被忽略");
    expect(!click(widget, 200, 200, pos, Qt::MiddleButton), "中键被忽略");
    expect(click(widget, 200, 200, pos) && pos.x == 2 && pos.y == 2, "左键仍映射到预期格子");

    // 非正方形窗口：留白在左右，判定与网格必须仍然一致。
    widget.resize(500, 402);
    app.processEvents();
    expect(!click(widget, 451, 200, pos), "宽窗口下超出右边缘的像素被忽略");
    expect(click(widget, 449, 200, pos) && pos.x == 2 && pos.y == 4,
           "宽窗口下棋盘内最后一列仍映射到 4");
    expect(!click(widget, 49, 200, pos), "宽窗口下左侧留白被忽略");

    widget.resize(402, 500);
    app.processEvents();
    expect(!click(widget, 200, 451, pos), "高窗口下超出下边缘的像素被忽略");
    expect(click(widget, 200, 449, pos) && pos.x == 4 && pos.y == 2,
           "高窗口下棋盘内最后一行仍映射到 4");

    return test_support::finish();
}
