#include "setup_dialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <array>

// 布阵对话框实现：三个坐标查询函数是核心，其余都是搭界面。
// 三角区坐标表必须与 Board::setup_custom 里的 kRedPos / kBluePos 逐项一致——
// get_index_in_region 返回的下标就是 layout 的下标，错一位就会把编号摆到别的格子上。
// 点击只在 (v % 6) + 1 上循环，编号重复留到“确认”时统一校验。
namespace ui {

SetupDialog::SetupDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("布阵阶段");
    // 固定尺寸：网格是 5 列各 60px 的按钮，窗口可缩放会把按钮压变形。
    // 相比只有阵营行时加高 40px，给“电脑强度”下拉框留出一行。
    setFixedSize(400, 560);

    // 0 是“尚未分配编号”的哨兵值，update_grid_ui 据此填初值。
    layout_.fill(0);

    auto* main_layout = new QVBoxLayout(this);

    // 阵营单选行：红方默认选中，右侧留白把整行推向左。
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

    // 电脑强度：条目全部由 ai::kAllLevels 生成，界面不写死档位数量与名称；
    // 用整数值存进 itemData，避免为枚举注册 Qt 元类型。
    auto* level_layout = new QHBoxLayout();
    level_layout->addWidget(new QLabel("电脑强度："));
    level_combo_ = new QComboBox(this);
    for (const ai::Level level : ai::kAllLevels) {
        level_combo_->addItem(QString::fromStdString(ai::level_name(level)),
                              static_cast<int>(level));
    }
    // 默认选中 kDefaultLevel（当前最新版），与“不指定版本就用最新版”的约定一致。
    level_combo_->setCurrentIndex(level_combo_->findData(static_cast<int>(ai::kDefaultLevel)));
    level_layout->addWidget(level_combo_);
    level_layout->addStretch();
    main_layout->addLayout(level_layout);
    main_layout->addSpacing(10);

    main_layout->addWidget(new QLabel("点击亮起的格子循环切换数字 1~6："));

    // 网格间距压到 2px，让 5x5 更接近一整块棋盘而不是散落的按钮。
    auto* grid_layout = new QGridLayout();
    grid_layout->setSpacing(2);

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            grid_btns_[i][j] = new QPushButton(this);
            grid_btns_[i][j]->setFixedSize(60, 60);
            grid_btns_[i][j]->setStyleSheet(
                "border-radius: 4px; font-size: 20px; font-weight: bold;");

            // i、j 按值捕获，否则所有按钮都会拿到循环结束后的同一份坐标。
            connect(grid_btns_[i][j], &QPushButton::clicked, this,
                    [this, i, j]() { on_grid_clicked(i, j); });

            // 网格行列直接映射到 Pos：第 i 行第 j 列，不做任何转置。
            grid_layout->addWidget(grid_btns_[i][j], i, j);
        }
    }
    main_layout->addLayout(grid_layout);
    main_layout->addSpacing(15);

    auto* confirm_btn = new QPushButton("确认布阵", this);
    confirm_btn->setStyleSheet(
        "padding: 12px; font-size: 16px; background-color: #2ecc71; font-weight: bold;");
    main_layout->addWidget(confirm_btn);

    connect(confirm_btn, &QPushButton::clicked, this, &SetupDialog::on_confirm_clicked);
    connect(red_radio_, &QRadioButton::toggled, this, &SetupDialog::on_side_changed);
    connect(blue_radio_, &QRadioButton::toggled, this, &SetupDialog::on_side_changed);

    // setChecked(true) 发生在 connect 之前，toggled 没发出来，这里手动跑一次初始化。
    on_side_changed();
}

bool SetupDialog::is_red_region(int x, int y) const {
    // 红方三角区：左上角三行，逐行缩短为 3 + 2 + 1 格。
    return (x == 0 && y <= 2) || (x == 1 && y <= 1) || (x == 2 && y == 0);
}

bool SetupDialog::is_blue_region(int x, int y) const {
    // 蓝方三角区：右下角三行，同样 3 + 2 + 1 格，与红方关于棋盘中心对称。
    return (x == 4 && y >= 2) || (x == 3 && y >= 3) || (x == 2 && y == 4);
}

int SetupDialog::get_index_in_region(int x, int y) const {
    // 下标顺序即 Board::setup_custom 的落位顺序：从靠近己方底角的格子起，逐行推进。
    // 红方第 0 项落 (0,0)，蓝方第 0 项落 (4,4)，两者关于中心一一对应。
    // 注意: 用哪张表取决于 red_radio_ 的当前状态而非坐标所属区域；
    //       不在当前表中的坐标一律返回 -1，调用方按 -1 过滤。
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
    // 换阵营等于换三角区，旧的六个编号在新的区里无处安放，直接清空重来。
    // 注意: 连的是 toggled，切换时两个单选按钮各发一次信号，本函数会被调用两次（幂等）。
    layout_.fill(0);
    update_grid_ui();
}

void SetupDialog::update_grid_ui() {
    bool is_red = red_radio_->isChecked();

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            bool active = is_red ? is_red_region(i, j) : is_blue_region(i, j);

            if (active) {
                int idx = get_index_in_region(i, j);
                // 初值填 idx + 1，让首次进入时看到的是 1~6 的排列而不是一片 0；
                // 玩家点击后就从这个值继续循环。
                if (layout_[idx] == 0) {
                    layout_[idx] = idx + 1;
                }
                grid_btns_[i][j]->setEnabled(true);
                grid_btns_[i][j]->setText(QString::number(layout_[idx]));

                // 活动格用阵营色区分（红 #e74c3c / 蓝 #3498db），文字用白色。
                grid_btns_[i][j]->setStyleSheet(
                    QString("background-color: %1; color: white; border-radius: 4px; font-size: "
                            "20px; font-weight: bold;")
                        .arg(is_red ? "#e74c3c" : "#3498db"));
            } else {
                // 非活动格：禁用、清空文字、压成深灰底，视觉上退到背景里去。
                grid_btns_[i][j]->setEnabled(false);
                grid_btns_[i][j]->setText("");

                grid_btns_[i][j]->setStyleSheet(
                    "background-color: #34495e; border-radius: 4px; border: none;");
            }
        }
    }
}

void SetupDialog::on_grid_clicked(int x, int y) {
    int idx = get_index_in_region(x, y);
    // 越界（点到了非活动格）直接忽略；正常情况下禁用按钮不会发出 clicked。
    if (idx < 0) {
        return;
    }

    // (v % 6) + 1 实现 1->2->…->6->1 的循环：取值域始终是 1~6，0 只作为“未分配”存在。
    // 注意: 这里只改被点的那一格，不会替别的格子避让，重复编号留到确认时校验。
    layout_[idx] = (layout_[idx] % 6) + 1;
    grid_btns_[x][y]->setText(QString::number(layout_[idx]));
}

void SetupDialog::on_confirm_clicked() {
    // seen 直接用编号当下标（1~6），索引 0 空置不用。
    bool seen[7] = {false};
    for (int i = 0; i < 6; ++i) {
        if (seen[layout_[i]]) {
            // 有重复就留在对话框里，让玩家继续点格子调整。
            QMessageBox::warning(this, "布阵错误", "棋子编号不能重复！请点击棋盘上的格子调整。");
            return;
        }
        seen[layout_[i]] = true;
    }
    // 只有六个编号互不重复才 accept()，exec() 因此返回 Accepted。
    accept();
}

int SetupDialog::get_selected_side() const {
    // 1/2 是 core::Side 中 Red/Blue 的枚举值，调用方按此映射，不做字符串比较。
    return red_radio_->isChecked() ? 1 : 2;
}

std::array<int, 6> SetupDialog::get_layout() const {
    return layout_;
}

ai::Level SetupDialog::get_selected_level() const {
    // currentData 一定是构造时写入的 ai::Level 整数值，因此可直接转回枚举。
    return static_cast<ai::Level>(level_combo_->currentData().toInt());
}

}  // namespace ui