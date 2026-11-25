// 骰子控件回归：合法点数必须画点，非法点数不能退化成“空白骰面”。
//
// 直接渲染到 QImage 后统计深红色像素，避免依赖系统字体或截图比对；
// “没有骰子”允许整帧空白，但 0/7 这类非法值必须画出一个可见的错误标记。

#include "components/dice_widget.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <cstddef>
#include <optional>

#include "test_support.h"

using test_support::expect;

namespace {

/// @brief 把骰子控件渲染到透明背景的 QImage，便于做像素级断言。
QImage render_dice(std::optional<int> value) {
    ui::DiceWidget widget;
    widget.resize(120, 120);
    widget.set_dice(value);

    QImage image(widget.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}

/// @brief 统计接近骰点深红色（#c0392b）的像素数，抗锯齿边缘也算在内。
int count_red_pixels(const QImage& image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() > 0 && color.red() > 150 && color.green() < 120 &&
                color.blue() < 120 && color.red() > color.green() + 60) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 没有点数时允许整帧空白，这是“本回合还没掷骰”的正常状态。
    expect(count_red_pixels(render_dice(std::nullopt)) == 0, "no dice renders no red marks");

    // 合法点数必须有可见骰点。
    expect(count_red_pixels(render_dice(3)) > 0, "valid dice renders dots");

    // 非法点数不能静默留下白色空骰面，否则和“没有骰子”难以区分。
    expect(count_red_pixels(render_dice(0)) > 0, "invalid dice 0 renders an error mark");
    expect(count_red_pixels(render_dice(7)) > 0, "invalid dice 7 renders an error mark");

    return test_support::finish();
}
