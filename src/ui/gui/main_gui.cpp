/// @file src/ui/gui/main_gui.cpp
/// @brief GUI 程序入口：建 QApplication、切工作目录、加载 QSS，再显示主窗口。
///
/// 与 CLI 的两点差异：工作目录固定到 exe 目录，存档库因此总落在 exe 旁边；
/// QSS 从 qrc 读取，读失败只打日志、不阻断启动。

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>

#include "main_window.h"

int main(int argc, char* argv[]) {
    // QApplication 必须先于任何 QWidget 构造，argc/argv 交给 Qt 处理命令行参数。
    QApplication app(argc, argv);
    // 切到 exe 目录：从 IDE 或快捷方式启动时 CWD 都不确定，不切的话相对路径的
    // kDefaultDbPath 会落到别处。代价是以启动目录为基准的相对路径全部失效。
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    // ":/style.qss" 是 qrc 里的资源路径，文件内容在编译期被打进二进制。
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        // 整份表样式一次性挂到 QApplication 上，所有窗口与对话框自动继承。
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
        qDebug() << "QSS 加载成功！";
    } else {
        // 样式失败不影响功能，界面退回 Qt 默认外观继续运行。
        qDebug() << "QSS 加载失败！找不到 :/style.qss";
    }

    // 主窗口放在栈上：exec() 返回后 main 结束，窗口随之析构。
    ui::MainWindow w;
    w.show();
    return app.exec();
}