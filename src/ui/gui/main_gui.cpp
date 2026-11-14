#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>

#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
        qDebug() << "QSS 加载成功！";
    } else {
        qDebug() << "QSS 加载失败！找不到 :/style.qss";
    }

    ui::MainWindow w;
    w.show();
    return app.exec();
}