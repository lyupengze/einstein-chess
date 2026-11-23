// Toast 淡入淡出回归：旧实现用 setWindowOpacity()/windowOpacity 动画，
// 而这两个只对顶层窗口生效，提示条是子控件，所以“淡入”实际根本没发生，
// 到时还直接 hide()，消失是硬切。
//
// 修复后透明度由容器上的 QGraphicsOpacityEffect 驱动：淡入到 1、停留、再淡出到 0 并隐藏。
// 这里直接读 effect 的 opacity 属性验证动画真的在跑，不依赖像素比较。

#include <QApplication>
#include <QEventLoop>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <cstdio>

#include "components/toast_widget.h"

namespace {

int failures = 0;

void expect(bool cond, const char* what) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) {
        ++failures;
    }
}

// 跑事件循环到指定时刻，让动画与定时器真的推进。
void spin(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QWidget parent;
    parent.resize(400, 300);
    parent.show();
    app.processEvents();

    ui::ToastWidget toast(&parent);
    auto* opacity = toast.findChild<QGraphicsOpacityEffect*>();
    expect(opacity != nullptr, "toast owns an opacity effect");
    if (opacity == nullptr) {
        return 1;
    }

    // 停留 300ms：先给 180ms 淡入留出余量，再在计时器到期前采样。
    toast.show_message("保存成功", 300);
    expect(toast.isVisible(), "toast becomes visible immediately");
    expect(toast.width() > 0 && toast.height() > 0, "toast is resized to its content");
    spin(250);
    expect(opacity->opacity() > 0.99, "fade-in reaches full opacity");

    // 300ms 计时器 + 200ms 淡出，留足余量后应已隐藏。
    spin(1200);
    expect(!toast.isVisible(), "toast hides after dwell plus fade-out");

    // 淡出途中再次 show：应重新可见并回到全不透明，而不是被旧动画继续拉向 0。
    toast.show_message("第一条", 200);
    spin(220);  // 计时器已到期，正在淡出
    toast.show_message("第二条", 3000);
    spin(300);
    expect(toast.isVisible(), "re-show during fade-out keeps the toast visible");
    expect(opacity->opacity() > 0.99, "re-show fades back to full opacity");

    std::printf(failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
