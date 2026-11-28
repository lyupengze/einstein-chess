/// @file src/ui/gui/dialogs/setup_dialog.cpp
/// @brief 布阵对话框的界面搭建与三角区坐标换算。
///
/// 三角区的形状与落位序号都由 core::Board 的初始区坐标表给出，这里只把网格下标
/// 转成 Pos 再查询，坐标表只保留 core::Board 一份。点击只在 (v % 6) + 1 上循环，
/// 重复编号留到确认时校验。

#include "setup_dialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <array>
#include <cstddef>

namespace gui {

namespace {

/// @brief 网格边长，取自 core::Board::kSize。
constexpr int kSide{static_cast<int>(core::Board::kSize)};
/// @brief 每方棋子数，取自 core::Board::kChessCount。
constexpr int kPieces{static_cast<int>(core::Board::kChessCount)};

/// @brief 把网格下标转成棋盘坐标；调用点已保证 x、y 落在 0~4。
core::Pos to_pos(int x, int y) {
    return {static_cast<std::size_t>(x), static_cast<std::size_t>(y)};
}

}  // namespace

SetupDialog::SetupDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("布阵阶段"));
    // 固定尺寸：网格是 5 列各 60px 的按钮，窗口可缩放会把按钮压变形。
    // 相比只有阵营行时加高 40px，给“电脑强度”下拉框留出一行。
    setFixedSize(400, 560);

    // 0 是“尚未分配编号”的哨兵值，update_grid_ui 据此填初值。
    layout_.fill(0);

    auto* main_layout = new QVBoxLayout(this);

    // 阵营单选行：红方默认选中，右侧留白把整行推向左。
    auto* side_layout = new QHBoxLayout();
    side_layout->addWidget(new QLabel(QStringLiteral("你的阵营：")));
    red_radio_ = new QRadioButton(QStringLiteral("红方 (先手 / 左上)"));
    blue_radio_ = new QRadioButton(QStringLiteral("蓝方 (后手 / 右下)"));
    red_radio_->setChecked(true);
    side_layout->addWidget(red_radio_);
    side_layout->addWidget(blue_radio_);
    side_layout->addStretch();
    main_layout->addLayout(side_layout);
    main_layout->addSpacing(10);

    // 电脑强度：条目全部由 ai::kAllLevels 生成，界面不写死档位数量与名称；
    // 用整数值存进 itemData，避免为枚举注册 Qt 元类型。
    auto* level_layout = new QHBoxLayout();
    level_layout->addWidget(new QLabel(QStringLiteral("电脑强度：")));
    level_combo_ = new QComboBox(this);
    for (const ai::Level level : ai::kAllLevels) {
        level_combo_->addItem(QString::fromStdString(ai::level_name(level)),
                              static_cast<int>(level));
    }
    // 默认选中 kDefaultLevel，与 ai::decide_move 的默认参数一致。
    level_combo_->setCurrentIndex(level_combo_->findData(static_cast<int>(ai::kDefaultLevel)));
    level_layout->addWidget(level_combo_);
    level_layout->addStretch();
    main_layout->addLayout(level_layout);
    main_layout->addSpacing(10);

    main_layout->addWidget(new QLabel(QStringLiteral("点击亮起的格子循环切换数字 1~6：")));

    // 网格间距压到 2px，让 5x5 更接近一整块棋盘而不是散落的按钮。
    auto* grid_layout = new QGridLayout();
    grid_layout->setSpacing(2);

    for (int i = 0; i < kSide; ++i) {
        for (int j = 0; j < kSide; ++j) {
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

    auto* confirm_btn = new QPushButton(QStringLiteral("确认布阵"), this);
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
    // 形状由 core::Board 的初始区表决定，这里只做一次查询。
    return core::Board::start_index(core::Side::Red, to_pos(x, y)).has_value();
}

bool SetupDialog::is_blue_region(int x, int y) const {
    return core::Board::start_index(core::Side::Blue, to_pos(x, y)).has_value();
}

int SetupDialog::get_index_in_region(int x, int y) const {
    // 用哪张表取决于当前阵营，而不是坐标属于哪个区：另一方的三角区在这里一律得到 -1。
    const core::Side side = red_radio_->isChecked() ? core::Side::Red : core::Side::Blue;
    return core::Board::start_index(side, to_pos(x, y)).value_or(-1);
}

void SetupDialog::on_side_changed() {
    // 换阵营等于换三角区，旧的六个编号在新的区里无处安放，直接清空重来。
    // 注意: 连的是 toggled，切换时两个单选按钮各发一次信号，本函数会被调用两次（幂等）。
    layout_.fill(0);
    update_grid_ui();
}

void SetupDialog::update_grid_ui() {
    bool is_red = red_radio_->isChecked();

    for (int i = 0; i < kSide; ++i) {
        for (int j = 0; j < kSide; ++j) {
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
                    QStringLiteral(
                        "background-color: %1; color: white; border-radius: 4px; font-size: "
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
    layout_[idx] = (layout_[idx] % kPieces) + 1;
    grid_btns_[x][y]->setText(QString::number(layout_[idx]));
}

void SetupDialog::on_confirm_clicked() {
    // seen 直接用编号当下标（1~6），索引 0 空置不用。
    std::array<bool, kPieces + 1> seen{};
    for (int i = 0; i < kPieces; ++i) {
        if (seen[layout_[i]]) {
            // 有重复就留在对话框里，让玩家继续点格子调整。
            QMessageBox::warning(this, QStringLiteral("布阵错误"),
                                 QStringLiteral("棋子编号不能重复，请点击棋盘上的格子调整。"));
            return;
        }
        seen[layout_[i]] = true;
    }
    // 只有六个编号互不重复才 accept()，exec() 因此返回 Accepted。
    accept();
}

int SetupDialog::get_selected_side() const {
    // 返回 1/2，与本对话框的约定一致；与 core::Side 的枚举数值没有语义关系，
    // 调用方按 1、2 显式映射，不做字符串比较。
    return red_radio_->isChecked() ? 1 : 2;
}

std::array<int, core::Board::kChessCount> SetupDialog::get_layout() const {
    return layout_;
}

ai::Level SetupDialog::get_selected_level() const {
    // currentData 一定是构造时写入的 ai::Level 整数值，因此可直接转回枚举。
    return static_cast<ai::Level>(level_combo_->currentData().toInt());
}

}  // namespace gui
