// Toast 淡入淡出回归：旧实现用 setWindowOpacity()/windowOpacity 动画，
// 而这两个只对顶层窗口生效，提示条是子控件，所以“淡入”实际根本没发生，
// 到时还直接 hide()，消失是硬切。
//
// 修复后透明度由容器上的 QGraphicsOpacityEffect 驱动：淡入到 1、停留、再淡出到 0 并隐藏。
// 这里直接读 effect 的 opacity 属性验证动画真的在跑，不依赖像素比较。
// 等待全部走条件轮询：动画快慢随机器变化，固定 sleep 只会把 flaky 藏起来。

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <iostream>
#include <string_view>

#include "components/toast_widget.h"
#include "qt_wait.h"

namespace {

int failures = 0;

void expect(bool cond, std::string_view what) {
    std::cout << (cond ? "PASS" : "FAIL") << ' ' << what << '\n';
    if (!cond) {
        ++failures;
    }
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

    // 停留 300ms：等真实动画到全不透明，而不是猜“180ms 淡入应该走完了”。
    toast.show_message("保存成功", 300);
    expect(toast.isVisible(), "toast becomes visible immediately");
    expect(toast.width() > 0 && toast.height() > 0, "toast is resized to its content");
    expect(test_support::wait_until([&] { return opacity->opacity() > 0.99; }, 2000,
                                    "fade-in reaches full opacity"),
           "fade-in reaches full opacity");

    // 300ms 计时器 + 200ms 淡出：等它自己隐藏，超时才失败。
    expect(test_support::wait_until([&] { return !toast.isVisible(); }, 3000,
                                    "toast hides after dwell plus fade-out"),
           "toast hides after dwell plus fade-out");

    // 淡出途中再次 show：先等第一次真的开始淡出，再插第二条，
    // 验证新消息把透明度拉回 1，而不是被旧动画继续拉向 0。
    toast.show_message("第一条", 200);
    expect(test_support::wait_until(
               [&] { return toast.isVisible() && opacity->opacity() < 0.99; }, 2000,
               "first toast starts fading out"),
           "first toast starts fading out");
    toast.show_message("第二条", 3000);
    expect(test_support::wait_until(
               [&] { return toast.isVisible() && opacity->opacity() > 0.99; }, 2000,
               "re-show fades back to full opacity"),
           "re-show during fade-out keeps the toast visible");
    expect(opacity->opacity() > 0.99, "re-show fades back to full opacity");

    if (failures == 0) {
        std::cout << "ALL PASS\n";
    } else {
        std::cout << failures << " FAILURE(S)\n";
    }
    return failures == 0 ? 0 : 1;
}
