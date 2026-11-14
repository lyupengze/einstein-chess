#include "menu_widget.h"

#include <QLabel>
#include <QVBoxLayout>

namespace ui {

MenuWidget::MenuWidget(QWidget* parent) : QWidget(parent) {
    this->setObjectName("menuBackground");
    this->setStyleSheet(
        "#menuBackground {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2c3e50, stop:1 #1a252f);"
        "}");

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("爱因斯坦棋", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "font-size: 56px; font-weight: bold; color: #ecf0f1; letter-spacing: 5px;");

    auto* subtitle = new QLabel("Einstein Chess", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(
        "font-size: 20px; color: #95a5a6; margin-bottom: 50px; font-style: italic;");

    start_btn_ = new QPushButton("开始新游戏", this);
    start_btn_->setFixedSize(240, 60);
    start_btn_->setStyleSheet("font-size: 18px;");

    load_btn_ = new QPushButton("读取存档", this);
    load_btn_->setFixedSize(240, 60);
    load_btn_->setStyleSheet("font-size: 18px; background-color: #7f8c8d;");

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(start_btn_, 0, Qt::AlignCenter);
    layout->addSpacing(20);
    layout->addWidget(load_btn_, 0, Qt::AlignCenter);

    auto* version = new QLabel("v1.0.0 | Made for C++ Course", this);
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 50px;");
    layout->addWidget(version);

    connect(start_btn_, &QPushButton::clicked, this, &MenuWidget::start_game_clicked);
    connect(load_btn_, &QPushButton::clicked, this, &MenuWidget::load_game_clicked);
}

}  // namespace ui