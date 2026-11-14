#include <QApplication>
#include <QFile>

#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 读取并应用样式表
    QFile styleFile("style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    ui::MainWindow w;
    w.show();
    return app.exec();
}