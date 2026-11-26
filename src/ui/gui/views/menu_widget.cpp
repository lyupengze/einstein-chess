#include "menu_widget.h"

#include <QLabel>
#include <QVBoxLayout>

/// @file src/ui/gui/views/menu_widget.cpp
/// @brief 搭主菜单页：标题、两个入口按钮与页脚版本号。
///
/// 纯界面搭建，没有状态机也不持有对局数据；背景渐变用 "#menuBackground" 限定在本页，
/// 按钮尺寸与页脚留白就地写死，style.qss 加载失败时版式也不会塌。

namespace ui {

MenuWidget::MenuWidget(QWidget* parent) : QWidget(parent) {
    // 先设 objectName，下面 "#menuBackground" 这条选择器才匹配得到本控件。
    this->setObjectName("menuBackground");
    // 渐变从左上 #2c3e50 到右下 #1a252f，与对局页的纯色背景同色系。
    this->setStyleSheet(
        "#menuBackground {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2c3e50, stop:1 #1a252f);"
        "}");

    // 顶层垂直布局：让整列内容在窗口里居中，而不是贴着左上角铺开。
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("爱因斯坦棋", this);
    title->setAlignment(Qt::AlignCenter);
    // 标题用 56px 加粗加字距，是本页唯一的视觉重点。
    title->setStyleSheet(
        "font-size: 56px; font-weight: bold; color: #ecf0f1; letter-spacing: 5px;");

    auto* subtitle = new QLabel("Einstein Chess", this);
    subtitle->setAlignment(Qt::AlignCenter);
    // 副标题的 margin-bottom 撑开与按钮组的距离，省掉一次 addSpacing。
    subtitle->setStyleSheet(
        "font-size: 20px; color: #95a5a6; margin-bottom: 50px; font-style: italic;");

    // 两个按钮固定 240x60：不交给布局商量，保证等宽等高。
    start_btn_ = new QPushButton("开始新游戏", this);
    start_btn_->setFixedSize(240, 60);
    start_btn_->setStyleSheet("font-size: 18px;");

    // 灰底表示次要操作，与主按钮的蓝色形成主次对比。
    load_btn_ = new QPushButton("读取存档", this);
    load_btn_->setFixedSize(240, 60);
    load_btn_->setStyleSheet("font-size: 18px; background-color: #7f8c8d;");

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(start_btn_, 0, Qt::AlignCenter);
    layout->addSpacing(20);
    layout->addWidget(load_btn_, 0, Qt::AlignCenter);

    // 页脚版本号：更小的字号、更暗的颜色，避免抢走按钮的注意力。
    // 版本来自 CMake 的 PROJECT_VERSION，避免升级时源码里的数字过期。
    auto* version = new QLabel(
        QStringLiteral("v%1 | Made for C++ Course").arg(QStringLiteral(EINSTEIN_VERSION)), this);
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 50px;");
    layout->addWidget(version);

    // 只把点击转发成信号，弹窗与切页都不在这里做，交给 MainWindow。
    connect(start_btn_, &QPushButton::clicked, this, &MenuWidget::start_game_clicked);
    connect(load_btn_, &QPushButton::clicked, this, &MenuWidget::load_game_clicked);
}

}  // namespace ui