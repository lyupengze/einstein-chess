/// @file tests/toast_fade_test.cpp
/// @brief Toast 淡入淡出回归：透明度由容器上的 QGraphicsOpacityEffect 驱动。
///
/// 淡入到 1、停留、再淡出到 0 并隐藏。windowOpacity 只对顶层窗口生效，提示条是
/// 子控件，因此透明度必须走 effect。测试直接读 effect 的 opacity 属性，不做像素
/// 比较；所有等待都走条件轮询，避免固定 sleep 把 flaky 藏起来。

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <iostream>
#include <string_view>

#include "components/toast_widget.h"
#include "qt_wait.h"
#include "test_support.h"

using test_support::expect;

namespace {}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QWidget parent;
    parent.resize(400, 300);
    parent.show();
    app.processEvents();

    ui::ToastWidget toast(&parent);
    auto* opacity = toast.findChild<QGraphicsOpacityEffect*>();
    expect(opacity != nullptr, "Toast 持有透明度效果");
    if (opacity == nullptr) {
        return 1;
    }

    // 停留 300ms：等真实动画到全不透明，不按固定时长猜测。
    toast.show_message("保存成功", 300);
    expect(toast.isVisible(), "Toast 立即显示");
    expect(toast.width() > 0 && toast.height() > 0, "Toast 尺寸随内容");
    expect(test_support::wait_until([&] { return opacity->opacity() > 0.99; }, 2000,
                                    "淡入到完全不透明"),
           "淡入到完全不透明");

    // 300ms 计时器 + 200ms 淡出：等它自己隐藏，超时才失败。
    expect(test_support::wait_until([&] { return !toast.isVisible(); }, 3000, "停留加淡出后隐藏"),
           "停留加淡出后隐藏");

    // 淡出途中再次 show：先等第一条完成淡入，再等它进入淡出（停留 400ms 之后），
    // 然后插第二条，验证新消息把透明度拉回 1，而不是被旧动画继续拉向 0。
    toast.show_message("第一条", 400);
    expect(test_support::wait_until([&] { return toast.isVisible() && opacity->opacity() > 0.99; },
                                    2000, "first toast finishes fading in"),
           "first toast finishes fading in");
    expect(test_support::wait_until([&] { return toast.isVisible() && opacity->opacity() < 0.99; },
                                    2000, "第一条开始淡出"),
           "第一条开始淡出");
    toast.show_message("第二条", 3000);
    expect(test_support::wait_until([&] { return toast.isVisible() && opacity->opacity() > 0.99; },
                                    2000, "重显后回到完全不透明"),
           "重显后回到完全不透明");
    expect(opacity->opacity() > 0.99, "re-show keeps the toast fully opaque");

    return test_support::finish();
}
