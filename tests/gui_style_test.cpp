/// @file tests/gui_style_test.cpp
/// @brief 样式表回归：加载真实 QSS 后，下拉框字体必须带点字号、展开候选列表不报警告。
///
/// QSS 用像素字号时，Qt 把字体记成 pixelSize 有效、pointSize 为 -1；下拉列表的
/// 弹出层继承这份字体后，QFont::setPointSize 会打印 "Point size <= 0 (-1)"。
/// 这个警告只在展开候选列表时出现，启动、布阵对话框与对局页都看不到，
/// 因此必须有用例盯着它，避免样式表改回 px 后无人察觉。成因与修复见
/// `#gui-combo-font-warning`。
///
/// 断言分两层：字体本身必须带点字号（根因，offscreen 也能稳定检查）；
/// 再展开一次候选列表，捕获这期间出现的 Qt 字体警告（真实路径）。

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFont>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <iostream>

#include "dialogs/setup_dialog.h"
#include "qt_wait.h"
#include "test_support.h"

using test_support::expect;

namespace {

/// @brief 测试期间收到的 Qt 警告与严重消息，按原始文本保存。
QStringList g_messages;

/// @brief 临时消息处理器：只记录警告及以上级别，调试信息不参与断言。
void capture_message(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        g_messages.append(message);
    }
}

/// @brief 判断一条消息是否由 QFont 的字号设置引发。
/// @param message Qt 消息原文
/// @return 命中 setPointSize 或 setPixelSize 时返回 true
bool is_font_size_warning(const QString& message) {
    return message.contains(QStringLiteral("setPointSize")) ||
           message.contains(QStringLiteral("setPixelSize"));
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // QSS 通过 resources.qrc 编进测试可执行文件，加载路径与产品一致。
    QFile style_file(QStringLiteral(":/style.qss"));
    const bool style_opened = style_file.open(QIODevice::ReadOnly | QIODevice::Text);
    expect(style_opened, "能读到 :/style.qss");
    if (style_opened) {
        app.setStyleSheet(QString::fromUtf8(style_file.readAll()));
    }

    const QtMessageHandler previous_handler = qInstallMessageHandler(capture_message);
    {
        gui::SetupDialog dialog;
        dialog.show();
        test_support::spin_events(20);

        auto* combo = dialog.findChild<QComboBox*>();
        expect(combo != nullptr, "布阵对话框里有强度下拉框");
        if (combo != nullptr) {
            // 根因断言：像素字号的 pointSize() 是 -1，弹出层套用这份字体时才会告警。
            const QFont combo_font = combo->font();
            std::cout << "下拉框字体: pointSize=" << combo_font.pointSize()
                      << " pixelSize=" << combo_font.pixelSize() << '\n';
            expect(combo_font.pointSize() > 0, "强度下拉框的字体带点字号");
            expect(combo_font.pixelSize() <= 0, "强度下拉框的字体不带像素字号");

            // 真实路径：展开候选列表，弹出层在这一步创建并继承样式字体。
            combo->showPopup();
            test_support::spin_events(50);
            combo->hidePopup();
            test_support::spin_events(10);
        }
    }
    qInstallMessageHandler(previous_handler);

    bool font_warning_seen = false;
    for (const QString& message : g_messages) {
        if (is_font_size_warning(message)) {
            font_warning_seen = true;
            std::cout << "捕获到字体警告: " << message.toStdString() << '\n';
        }
    }
    expect(!font_warning_seen, "展开强度下拉框不触发 QFont 字号警告");

    return test_support::finish();
}
