#include "setup_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace ui {

SetupDialog::SetupDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("布阵阶段");
    setFixedSize(400, 520);

    layout_.fill(0);  // 初始全为 0，表示未放棋子

    auto* main_layout = new QVBoxLayout(this);

    // 1. 选色
    auto* side_layout = new QHBoxLayout();
    side_layout->addWidget(new QLabel("你的阵营："));
    red_radio_ = new QRadioButton("红方 (先手 / 左上)");
    blue_radio_ = new QRadioButton("蓝方 (后手 / 右下)");
    red_radio_->setChecked(true);
    side_layout->addWidget(red_radio_);
    side_layout->addWidget(blue_radio_);
    side_layout->addStretch();
    main_layout->addLayout(side_layout);
    main_layout->addSpacing(10);

    main_layout->addWidget(new QLabel("点击亮起的格子循环切换数字 1~6："));

    // 2. 微型 5x5 棋盘
    auto* grid_layout = new QGridLayout();
    grid_layout->setSpacing(2);

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            grid_btns_[i][j] = new QPushButton(this);
            grid_btns_[i][j]->setFixedSize(60, 60);
            grid_btns_[i][j]->setStyleSheet(
                "border-radius: 4px; font-size: 20px; font-weight: bold;");

            // 绑定点击事件，注意捕获 i, j
            connect(grid_btns_[i][j], &QPushButton::clicked, this,
                    [this, i, j]() { on_grid_clicked(i, j); });

            grid_layout->addWidget(grid_btns_[i][j], i, j);
        }
    }
    main_layout->addLayout(grid_layout);
    main_layout->addSpacing(15);

    // 3. 确认按钮
    auto* confirm_btn = new QPushButton("确认布阵", this);
    confirm_btn->setStyleSheet(
        "padding: 12px; font-size: 16px; background-color: #2ecc71; font-weight: bold;");
    main_layout->addWidget(confirm_btn);

    connect(confirm_btn, &QPushButton::clicked, this, &SetupDialog::on_confirm_clicked);
    connect(red_radio_, &QRadioButton::toggled, this, &SetupDialog::on_side_changed);
    connect(blue_radio_, &QRadioButton::toggled, this, &SetupDialog::on_side_changed);

    on_side_changed();  // 初始化 UI
}

bool SetupDialog::is_red_region(int x, int y) const {
    return (x == 0 && y <= 2) || (x == 1 && y <= 1) || (x == 2 && y == 0);
}

bool SetupDialog::is_blue_region(int x, int y) const {
    return (x == 4 && y >= 2) || (x == 3 && y >= 3) || (x == 2 && y == 4);
}

int SetupDialog::get_index_in_region(int x, int y) const {
    // 把二维坐标映射到 0~5 的索引
    if (red_radio_->isChecked()) {
        if (x == 0 && y == 0) {
            return 0;
        }
        if (x == 0 && y == 1) {
            return 1;
        }
        if (x == 0 && y == 2) {
            return 2;
        }
        if (x == 1 && y == 0) {
            return 3;
        }
        if (x == 1 && y == 1) {
            return 4;
        }
        if (x == 2 && y == 0) {
            return 5;
        }
    } else {
        if (x == 4 && y == 4) {
            return 0;
        }
        if (x == 4 && y == 3) {
            return 1;
        }
        if (x == 4 && y == 2) {
            return 2;
        }
        if (x == 3 && y == 4) {
            return 3;
        }
        if (x == 3 && y == 3) {
            return 4;
        }
        if (x == 2 && y == 4) {
            return 5;
        }
    }
    return -1;
}

void SetupDialog::on_side_changed() {
    layout_.fill(0);  // 切换颜色，重置布阵
    update_grid_ui();
}

void SetupDialog::update_grid_ui() {
    bool is_red = red_radio_->isChecked();

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            bool active = is_red ? is_red_region(i, j) : is_blue_region(i, j);

            if (active) {
                int idx = get_index_in_region(i, j);
                if (layout_[idx] == 0) {
                    layout_[idx] = idx + 1;  // 默认填入 1~6
                }
                grid_btns_[i][j]->setEnabled(true);
                grid_btns_[i][j]->setText(QString::number(layout_[idx]));
                // 激活的格子用红/蓝色显示
                grid_btns_[i][j]->setStyleSheet(
                    QString("background-color: %1; color: white; border-radius: 4px; font-size: "
                            "20px; font-weight: bold;")
                        .arg(is_red ? "#e74c3c" : "#3498db"));
            } else {
                grid_btns_[i][j]->setEnabled(false);
                grid_btns_[i][j]->setText("");
                // 不可用区域用深色背景
                grid_btns_[i][j]->setStyleSheet(
                    "background-color: #34495e; border-radius: 4px; border: none;");
            }
        }
    }
}

void SetupDialog::on_grid_clicked(int x, int y) {
    int idx = get_index_in_region(x, y);
    if (idx < 0) {
        return;
    }

    // 数字循环 1~6
    layout_[idx] = (layout_[idx] % 6) + 1;
    grid_btns_[x][y]->setText(QString::number(layout_[idx]));
}

void SetupDialog::on_confirm_clicked() {
    bool seen[7] = {false};
    for (int i = 0; i < 6; ++i) {
        if (seen[layout_[i]]) {
            QMessageBox::warning(this, "布阵错误", "棋子编号不能重复！请点击棋盘上的格子调整。");
            return;
        }
        seen[layout_[i]] = true;
    }
    accept();
}

int SetupDialog::get_selected_side() const {
    return red_radio_->isChecked() ? 1 : 2;
}

std::array<int, 6> SetupDialog::get_layout() const {
    return layout_;
}

}  // namespace ui