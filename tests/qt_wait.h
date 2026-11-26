#ifndef TESTS_QT_WAIT_H_
#define TESTS_QT_WAIT_H_

// GUI 回归的条件等待工具。
//
// 旧用例用固定 sleep 猜“界面应该已经走到哪一步”，在慢机器或并行跑测试时
// 只能靠加长等待来压住 flaky；这里改成轮询真实状态，条件一满足就返回，
// 超时才失败，并打印等待目标。所有函数都要求当前线程已有 QApplication。

#include <QEventLoop>
#include <QTimer>
#include <chrono>
#include <functional>
#include <iostream>
#include <string_view>

namespace test_support {

/// @brief 让 Qt 事件循环运行一小段时间，供轮询函数推进定时器与动画。
/// @param milliseconds 本次转动的毫秒数
inline void spin_events(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

/// @brief 轮询等待 predicate 变为 true。
/// @param predicate 目标条件，每次事件循环转动后重新求值
/// @param timeout_ms 最长等待毫秒数
/// @param what 超时信息里要打印的等待目标
/// @return 超时前条件成立返回 true，否则打印 TIMEOUT 并返回 false
inline bool wait_until(const std::function<bool()>& predicate, int timeout_ms,
                       std::string_view what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        if (predicate()) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            std::cout << "TIMEOUT: " << what << " (waited " << timeout_ms << " ms)\n";
            return false;
        }
        spin_events(5);
    }
}

/// @brief 在给定时间窗内持续要求 predicate 为 false。
///
/// 用于“旧定时器不该再触发”这类否定断言：不能只睡一次就断言，
/// 必须让事件循环在整个窗口里都跑起来，才有机会看到迟到回调。
/// @param predicate 不应发生的条件
/// @param duration_ms 观察窗口的毫秒数
/// @param what 条件意外成立时要打印的说明
/// @return 窗口内始终为 false 返回 true；一旦为 true 立即打印并返回 false
inline bool expect_remains_false(const std::function<bool()>& predicate, int duration_ms,
                                 std::string_view what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            std::cout << "UNEXPECTED: " << what << " happened within " << duration_ms << " ms\n";
            return false;
        }
        spin_events(5);
    }
    if (predicate()) {
        std::cout << "UNEXPECTED: " << what << " happened at the edge of " << duration_ms
                  << " ms\n";
        return false;
    }
    return true;
}

}  // namespace test_support

#endif  // TESTS_QT_WAIT_H_